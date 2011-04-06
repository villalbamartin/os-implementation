/*
 * ELF executable loading
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2003, David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.29 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/errno.h>
#include <geekos/kassert.h>
#include <geekos/ktypes.h>
#include <geekos/screen.h>  /* for debug Print() statements */
#include <geekos/pfat.h>
#include <geekos/malloc.h>
#include <geekos/string.h>
#include <geekos/elf.h>


/**
 * From the data of an ELF executable, determine how its segments
 * need to be loaded into memory.
 * @param exeFileData buffer containing the executable file
 * @param exeFileLength length of the executable file in bytes
 * @param exeFormat structure describing the executable's segments
 *   and entry address; to be filled in
 * @return 0 if successful, < 0 on error
 */
int Parse_ELF_Executable(char *exeFileData, ulong_t exeFileLength,
    struct Exe_Format *exeFormat)
{
    elfHeader	encabezado;	// Encabezado del ELF
    programHeader ph;           // Program Header
    char*       offsets;        // Variable para usar memcpy
    char        comparar[7];    // Variable para validar el header
    int		i=0;		// Variable dummy
    
    // Primero cargo el header del elf, para ver sus propiedades
    // Copio los primeros 52 bytes del mismo en la estructura
    // (que ocupa lo mismo, segun la especificacion del formato ELF)
    // Esta es la forma vaga: memcpy(&encabezado,exeFileData,52);
    offsets=exeFileData;
    memcpy(&encabezado.ident, offsets, 16);    offsets+=16;
    memcpy(&encabezado.type, offsets, 2);      offsets+=2;
    memcpy(&encabezado.machine, offsets, 2);   offsets+=2;
    memcpy(&encabezado.version, offsets, 4);   offsets+=4;
    memcpy(&encabezado.entry, offsets, 4);     offsets+=4;
    memcpy(&encabezado.phoff, offsets, 4);     offsets+=4;
    memcpy(&encabezado.sphoff, offsets, 4);    offsets+=4;
    memcpy(&encabezado.flags, offsets, 4);     offsets+=4;
    memcpy(&encabezado.ehsize, offsets, 2);    offsets+=2;
    memcpy(&encabezado.phentsize, offsets, 2); offsets+=2;
    memcpy(&encabezado.phnum, offsets, 2);     offsets+=2;
    memcpy(&encabezado.shentsize, offsets, 2); offsets+=2;
    memcpy(&encabezado.shnum, offsets, 2);     offsets+=2;
    memcpy(&encabezado.shstrndx, offsets, 2);

    comparar[0]=0x7f; // Identificador de ELF
    comparar[1]='E';
    comparar[2]='L';
    comparar[3]='F';
    comparar[4]=0x01; // 32 bits
    comparar[5]=1;    // LSB
    comparar[6]=1;    // Version de ELF
    if(memcmp(&encabezado.ident,&comparar,7) != 0 ||
       encabezado.type != 0x02 || encabezado.machine != 0x03 ||
       encabezado.version != 0x01 || encabezado.phoff == 0 ||
       encabezado.phnum == 0 )
    {
        // No es un ejecutable correcto, puede ser desde
        // una librería hasta un ejecutable de otra arquitectura.
        return -1;
    }

    // Cargamos el Program Header.
    exeFormat->numSegments = encabezado.phnum;

    // Cargamos el primer dato de la estructura:
    // el code entry point
    exeFormat->entryAddr = encabezado.entry;

    // Rellenamos ahora las estructuras que definen los segmentos
    for(i=0; i<encabezado.phnum; i++)
    {
        // Este kernel no soporta más que 3 segmentos,
        // así que fallamos si hay más que eso
        KASSERT(i<3);

        // Primero, calculo el offset necesario hasta la entrada
        // en la tabla del Program Header donde está el segmento
        // que estoy cargando
        offsets = exeFileData + encabezado.phoff + encabezado.phentsize*i;

        // Cargo ahora los datos de la entrada actual en el Program Header
        // Esta es la forma vaga: memcpy(&ph, offsets, encabezado.phentsize);
        memcpy(&ph.type, offsets, 4);     offsets+=4;
        memcpy(&ph.offset, offsets, 4);   offsets+=4;
        memcpy(&ph.vaddr, offsets, 4);    offsets+=4;
        memcpy(&ph.paddr, offsets, 4);    offsets+=4;
        memcpy(&ph.fileSize, offsets, 4); offsets+=4;
        memcpy(&ph.memSize, offsets, 4);  offsets+=4;
        memcpy(&ph.flags, offsets, 4);    offsets+=4;
        memcpy(&ph.alignment, offsets, 4);

        // Ahora que tengo completa esta entrada del Program Header,
        // puedo rellenar los segmentos
        exeFormat->segmentList[i].offsetInFile=ph.offset;
        exeFormat->segmentList[i].lengthInFile=ph.fileSize;
        exeFormat->segmentList[i].startAddress=ph.vaddr;
        exeFormat->segmentList[i].sizeInMemory=ph.memSize;
        exeFormat->segmentList[i].protFlags=ph.flags;
    }

    return 0;
}


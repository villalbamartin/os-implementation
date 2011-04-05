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
    programHeader ph; // Program Header
    char*       offsets; // Variable para usar memcpy
    int		i=0;		// Variable dummy
    
    // Primero cargo el header del elf, para ver sus propiedades
    // Copio los primeros 52 bytes del mismo en la estructura
    // (que ocupa lo mismo, segun la especificacion del formato ELF)
    memcpy(&encabezado,exeFileData,52);
    if(encabezado.type != 0x02)
    {
        // No es un ejecutable, es una librería u otra cosa
        // Notar que muchas otras cosas podrían fallar acá,
        // pero las estamos ignorando
        return -1;
    }
    
    if((exeFormat->numSegments = encabezado.phnum) == 0 || encabezado.phoff == 0)
    {
        // No tiene Program Header
        return -1;
    }
    exeFormat->entryAddr = encabezado.entry;
    // Rellenamos la estructura
    for(i=0; i<encabezado.phnum; i++)
    {
        // Primero, extraigo la estructura del Program Header
        offsets = exeFileData;
        offsets += encabezado.phoff;
        offsets += encabezado.phentsize*i;

        // Chequear la alineacion creo que no hace falta,
        // por ser en tiempo de ejecucion
        memcpy(&ph, offsets, encabezado.phentsize);

        // Ahora que tengo completo el Program Header,
        // puedo rellenar los segmentos
        exeFormat->segmentList[i].offsetInFile=ph.offset;
        exeFormat->segmentList[i].lengthInFile=ph.fileSize;
        exeFormat->segmentList[i].startAddress=ph.vaddr;
        exeFormat->segmentList[i].sizeInMemory=ph.memSize;
        exeFormat->segmentList[i].protFlags=ph.flags;
    }

    return 0;
}


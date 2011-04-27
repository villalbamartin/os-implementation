/*
 * Segmentation-based user mode implementation
 * Copyright (c) 2001,2003 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.23 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/ktypes.h>
#include <geekos/kassert.h>
#include <geekos/defs.h>
#include <geekos/mem.h>
#include <geekos/string.h>
#include <geekos/malloc.h>
#include <geekos/int.h>
#include <geekos/gdt.h>
#include <geekos/segment.h>
#include <geekos/tss.h>
#include <geekos/kthread.h>
#include <geekos/argblock.h>
#include <geekos/user.h>

/* ----------------------------------------------------------------------
 * Variables
 * ---------------------------------------------------------------------- */

#define DEFAULT_USER_STACK_SIZE 8192


/* ----------------------------------------------------------------------
 * Private functions
 * ---------------------------------------------------------------------- */


/*
 * Create a new user context of given size
 */

static struct User_Context* Create_User_Context(ulong_t size)
{
	struct User_Context* uContext = NULL;

    /* Create a User_Context structure */
	uContext = Malloc(sizeof(struct User_Context));
	if(uContext!=NULL)
	{
	    uContext->memory = Malloc(sizeof(char)*size);
        if(uContext->memory!=NULL)
        {
            memset((char*) uContext->memory, '\0', size);
            uContext->size = size;

            /* Allocate an LDT descriptor in the GDT,
             * and initialize the LDT in the GDT
             * The first argument is right, just trust me,
             * the second one is the only one that makes sense
             */
            uContext->ldtDescriptor = Allocate_Segment_Descriptor();
            if(uContext->ldtDescriptor != NULL)
            {
                Init_LDT_Descriptor(uContext->ldtDescriptor,uContext->ldt,NUM_USER_LDT_ENTRIES);

                /* Create a selector, don't really know why */
                uContext->ldtSelector = Selector(KERNEL_PRIVILEGE, 1==1, Get_Descriptor_Index(uContext->ldtDescriptor));

                /* Initialize the descriptors in LDT */
                Init_Code_Segment_Descriptor(&uContext->ldtDescriptor[0], (unsigned long) uContext->memory, (size/PAGE_SIZE)+10, USER_PRIVILEGE);
                Init_Data_Segment_Descriptor(&uContext->ldtDescriptor[1], (unsigned long) uContext->memory, (size/PAGE_SIZE)+10, USER_PRIVILEGE);

                /* Create remaining selectors, inside the LDT */
                uContext->csSelector = Selector(USER_PRIVILEGE, 1==0, 0);
                uContext->dsSelector = Selector(USER_PRIVILEGE, 1==0, 1);
            }
            else
            {
                Free(uContext->memory);
                Free(uContext);
                uContext = NULL;
            }
        }
        else
        {
            Free(uContext);
            uContext = NULL;
        }
	}
    return uContext;
}


static bool Validate_User_Memory(struct User_Context* userContext,
    ulong_t userAddr, ulong_t bufSize)
{
    ulong_t avail;

    if (userAddr >= userContext->size)
        return false;

    avail = userContext->size - userAddr;
    if (bufSize > avail)
        return false;

    return true;
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

/*
 * Destroy a User_Context object, including all memory
 * and other resources allocated within it.
 */
void Destroy_User_Context(struct User_Context* userContext)
{
    /*
     * Hints:
     * - you need to free the memory allocated for the user process
     * - don't forget to free the segment descriptor allocated
     *   for the process's LDT
     */
    TODO("Destroy a User_Context");
}

/*
 * Load a user executable into memory by creating a User_Context
 * data structure.
 * Params:
 * exeFileData - a buffer containing the executable to load
 * exeFileLength - number of bytes in exeFileData
 * exeFormat - parsed ELF segment information describing how to
 *   load the executable's text and data segments, and the
 *   code entry point address
 * command - string containing the complete command to be executed:
 *   this should be used to create the argument block for the
 *   process
 * pUserContext - reference to the pointer where the User_Context
 *   should be stored
 *
 * Returns:
 *   0 if successful, or an error code (< 0) if unsuccessful
 */
int Load_User_Program(char *exeFileData, ulong_t exeFileLength,
    struct Exe_Format *exeFormat, const char *command,
    struct User_Context **pUserContext)
{
    /*
     * Hints:
     * - Determine where in memory each executable segment will be placed
     * - Determine size of argument block and where it memory it will
     *   be placed
     * - Copy each executable segment into memory
     * - Format argument block in memory
     * - In the created User_Context object, set code entry point
     *   address, argument block address, and initial kernel stack pointer
     *   address
     */

    /* If I've ever written something likely to have off-by-1 errors, this is it */
    int i=0;                         /* Tradition dictates this should be a char! */
    int retval=0;                    /* Return value */

    struct User_Context *uCont=NULL; /* User context */
    ulong_t reqMem = 0;              /* Required memory */
    ulong_t stackBegin = 0;          /* Beginning of stack (and argument block) */

    struct Argument_Block *argBlock=NULL; /* Temporary argument block */
    ulong_t argBSize = 0;                 /* Size of argument block */
    unsigned int numargs = 0;             /* Number of arguments in argument block */


    /* First, let's parse the arguments */
    Get_Argument_Block_Size(command, &numargs, &argBSize);

    /* Now, lets calculate how much memory we should allocate */
    /* If I'm not mistaken, the largest virtual address shouldn't be
     * larger than max(base_address)+size
     * This code handles the superposition of both segments
     */
    reqMem = 0;
    for(i=0; i< exeFormat->numSegments; i++)
    {
        if(exeFormat->segmentList[i].startAddress + exeFormat->segmentList[i].sizeInMemory > reqMem)
        {
            reqMem = exeFormat->segmentList[i].startAddress + exeFormat->segmentList[i].sizeInMemory;
        }
    }

    /* Keep a record of memory sizes and locations */
    stackBegin = Round_Up_To_Page(reqMem)+ Round_Up_To_Page(DEFAULT_USER_STACK_SIZE);
    reqMem = stackBegin + Round_Up_To_Page(argBSize);

    /* We can now create our User Context structure */
    uCont = Create_User_Context(reqMem);
    if(uCont!=NULL)
    {
        uCont->entryAddr = exeFormat->entryAddr;
        uCont->refCount=0;

        /* Let's copy now to memory the segments */
        memcpy( (void*) uCont->memory+exeFormat->segmentList[0].startAddress,
                (void *) exeFileData + exeFormat->segmentList[0].offsetInFile,
                exeFormat->segmentList[0].lengthInFile);
        memcpy( (void*) uCont->memory+exeFormat->segmentList[1].startAddress,
                (void *) exeFileData + exeFormat->segmentList[1].offsetInFile,
                exeFormat->segmentList[1].lengthInFile);

        /* And now, we copy the the argument block */
        argBlock = (struct Argument_Block *)Malloc(argBSize);
        if(argBlock!=NULL)
        {
            Format_Argument_Block((char*) argBlock, numargs, reqMem - argBSize, command);
            memcpy( (void*) uCont->memory+stackBegin, (void *) argBlock, argBSize);

            /* Let's fill the remaining fields in the User Context */
            uCont->argBlockAddr = stackBegin;
            uCont->stackPointerAddr = stackBegin;
        }
        else
        {
            Free(uCont);
            retval = -1;
        }
    }
    else
    {
        retval = -1;
    }
    return retval;
}

/*
 * Copy data from user memory into a kernel buffer.
 * Params:
 * destInKernel - address of kernel buffer
 * srcInUser - address of user buffer
 * bufSize - number of bytes to copy
 *
 * Returns:
 *   true if successful, false if user buffer is invalid (i.e.,
 *   doesn't correspond to memory the process has a right to
 *   access)
 */
bool Copy_From_User(void* destInKernel, ulong_t srcInUser, ulong_t bufSize)
{
    /*
     * Hints:
     * - the User_Context of the current process can be found
     *   from g_currentThread->userContext
     * - the user address is an index relative to the chunk
     *   of memory you allocated for it
     * - make sure the user buffer lies entirely in memory belonging
     *   to the process
     */
    TODO("Copy memory from user buffer to kernel buffer");
    Validate_User_Memory(NULL,0,0); /* delete this; keeps gcc happy */
}

/*
 * Copy data from kernel memory into a user buffer.
 * Params:
 * destInUser - address of user buffer
 * srcInKernel - address of kernel buffer
 * bufSize - number of bytes to copy
 *
 * Returns:
 *   true if successful, false if user buffer is invalid (i.e.,
 *   doesn't correspond to memory the process has a right to
 *   access)
 */
bool Copy_To_User(ulong_t destInUser, void* srcInKernel, ulong_t bufSize)
{
    /*
     * Hints: same as for Copy_From_User()
     */
    TODO("Copy memory from kernel buffer to user buffer");
}

/*
 * Switch to user address space belonging to given
 * User_Context object.
 * Params:
 * userContext - the User_Context
 */
void Switch_To_Address_Space(struct User_Context *userContext)
{
    /*
     * Hint: you will need to use the lldt assembly language instruction
     * to load the process's LDT by specifying its LDT selector.
     */

    /* They said this is the way, so who am I to argue? */
    __asm__ __volatile__ ("lldt %0" :: "a" (userContext->ldtSelector));
}


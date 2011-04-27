/*
 * Common user mode functions
 * Copyright (c) 2001,2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.50 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/errno.h>
#include <geekos/ktypes.h>
#include <geekos/kassert.h>
#include <geekos/int.h>
#include <geekos/mem.h>
#include <geekos/malloc.h>
#include <geekos/kthread.h>
#include <geekos/vfs.h>
#include <geekos/tss.h>
#include <geekos/user.h>

/*
 * This module contains common functions for implementation of user
 * mode processes.
 */

/*
 * Associate the given user context with a kernel thread.
 * This makes the thread a user process.
 */
void Attach_User_Context(struct Kernel_Thread* kthread, struct User_Context* context)
{
    KASSERT(context != 0);
    kthread->userContext = context;

    Disable_Interrupts();

    /*
     * We don't actually allow multiple threads
     * to share a user context (yet)
     */
    KASSERT(context->refCount == 0);

    ++context->refCount;
    Enable_Interrupts();
}

/*
 * If the given thread has a user context, detach it
 * and destroy it.  This is called when a thread is
 * being destroyed.
 */
void Detach_User_Context(struct Kernel_Thread* kthread)
{
    struct User_Context* old = kthread->userContext;

    kthread->userContext = 0;

    if (old != 0) {
	int refCount;

	Disable_Interrupts();
        --old->refCount;
	refCount = old->refCount;
	Enable_Interrupts();

        if (refCount == 0)
            Destroy_User_Context(old);
    }
}

/*
 * Spawn a user process.
 * Params:
 *   program - the full path of the program executable file
 *   command - the command, including name of program and arguments
 *   pThread - reference to Kernel_Thread pointer where a pointer to
 *     the newly created user mode thread (process) should be
 *     stored
 * Returns:
 *   The process id (pid) of the new process, or an error code
 *   if the process couldn't be created.  Note that this function
 *   should return ENOTFOUND if the reason for failure is that
 *   the executable file doesn't exist.
 */
int Spawn(const char *program, const char *command, struct Kernel_Thread **pThread)
{
    /*
     * Hints:
     * - Call Read_Fully() to load the entire executable into a memory buffer
     * - Call Parse_ELF_Executable() to verify that the executable is
     *   valid, and to populate an Exe_Format data structure describing
     *   how the executable should be loaded
     * - Call Load_User_Program() to create a User_Context with the loaded
     *   program
     * - Call Start_User_Thread() with the new User_Context
     *
     * If all goes well, store the pointer to the new thread in
     * pThread and return 0.  Otherwise, return an error code.
     */

    int retval = -1;                           /* Return value */
    char* pData = NULL;                        /* Program data */
    ulong_t pLen = 0;                          /* Program length */
    struct Exe_Format* pStructure=NULL;        /* Structure to store the program */
    struct User_Context* pContext=NULL;        /* User Context for the program */
    struct Kernel_Thread* pThreadPtr = NULL;   /* New Kernel Thread */

    pContext=(struct User_Context*)Malloc(sizeof(struct User_Context));
    pStructure=(struct Exe_Format*)Malloc(sizeof(struct Exe_Format));
    if(pContext != NULL && pStructure != NULL)
    {
        /* Load program onto buffer */
        if(Read_Fully(program, (void**) &pData, &pLen) == 0)
        {
            /* Parse ELF Executable, filling the internal structure */
            if(Parse_ELF_Executable(pData, pLen, pStructure) == 0)
            {
                /* Create the user context with the loaded program */
                if (Load_User_Program(pData, pLen, pStructure, command, (struct User_Context**) pContext) == 0)
                {
                    /* Kernel thread with the new context
                     * Since we hate multitasking, "detached" will always be false,
                     * unless we find a good reason to change this
                     * or someone realizes we are doing things the lazy way
                     */
                    pThreadPtr = Start_User_Thread(pContext, 0==1);
                    if(pThreadPtr != NULL)
                    {
                        // Let's return the thread as it should be
                        *pThread = pThreadPtr;
                        retval = pThreadPtr->pid;
                    }
                    else
                    {
                        Free(pStructure);
                        Free(pData);
                        Free(pContext);
                    }
                }
                else
                {
                    Free(pData);
                    Free(pStructure);
                    Free(pContext);
                }
            }
            else
            {
                Free(pData);
                Free(pStructure);
                Free(pContext);
            }
        }
        else
        {
            /* This is the only special error,
             * the "File Not Found" error */
            retval = ENOTFOUND;
            Free(pStructure);
            Free(pContext);
        }
    }
    else
    {
        Free(pContext);
        Free(pStructure);
    }
    return retval;
}

/*
 * If the given thread has a User_Context,
 * switch to its memory space.
 *
 * Params:
 *   kthread - the thread that is about to execute
 *   state - saved processor registers describing the state when
 *      the thread was interrupted
 */
void Switch_To_User_Context(struct Kernel_Thread* kthread, struct Interrupt_State* state)
{
    /*
     * Hint: Before executing in user mode, you will need to call
     * the Set_Kernel_Stack_Pointer() and Switch_To_Address_Space()
     * functions.
     */
    if(kthread->userContext != NULL)
    {
        /* User thread, requires context switching
         * Probably, we should just run everything on kernel space,
         * and make our lives easier
         */
        //TODO("Switch PROPERLY to a new user address space, if necessary, which it is");

        Set_Kernel_Stack_Pointer(((ulong_t) kthread->stackPage) + PAGE_SIZE);
        Switch_To_Address_Space(kthread->userContext);
    }
}


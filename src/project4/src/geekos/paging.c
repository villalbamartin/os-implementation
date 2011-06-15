/*
 * Paging (virtual memory) support
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2003,2004 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.55 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/string.h>
#include <geekos/int.h>
#include <geekos/idt.h>
#include <geekos/kthread.h>
#include <geekos/kassert.h>
#include <geekos/screen.h>
#include <geekos/mem.h>
#include <geekos/malloc.h>
#include <geekos/gdt.h>
#include <geekos/segment.h>
#include <geekos/user.h>
#include <geekos/vfs.h>
#include <geekos/crc32.h>
#include <geekos/paging.h>

/* ----------------------------------------------------------------------
 * Public data
 * ---------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 * Private functions/data
 * ---------------------------------------------------------------------- */

#define SECTORS_PER_PAGE (PAGE_SIZE / SECTOR_SIZE)

/*
 * flag to indicate if debugging paging code
 */
int debugFaults = 0;
#define Debug(args...) if (debugFaults) Print(args)


void checkPaging()
{
  unsigned long reg=0;
  /*__asm__ __volatile__( "movl %%cr0, %0" : "=a" (reg));*/
  __asm__ __volatile__( "mov %%cr0, %0" : "=a" (reg));
  Print("Paging on ? : %d\n", (reg & (1<<31)) != 0);
}


/*
 * Print diagnostic information for a page fault.
 */
static void Print_Fault_Info(uint_t address, faultcode_t faultCode)
{
    extern uint_t g_freePageCount;

    Print("Pid %d, Page Fault received, at address %x (%d pages free)\n",
        g_currentThread->pid, address, g_freePageCount);
    if (faultCode.protectionViolation)
        Print ("   Protection Violation, ");
    else
        Print ("   Non-present page, ");
    if (faultCode.writeFault)
        Print ("Write Fault, ");
    else
        Print ("Read Fault, ");
    if (faultCode.userModeFault)
        Print ("in User Mode\n");
    else
        Print ("in Supervisor Mode\n");
}

/*
 * Handler for page faults.
 * You should call the Install_Interrupt_Handler() function to
 * register this function as the handler for interrupt 14.
 */
/*static*/ void Page_Fault_Handler(struct Interrupt_State* state)
{
    ulong_t address;
    faultcode_t faultCode;

    KASSERT(!Interrupts_Enabled());

    /* Get the address that caused the page fault */
    address = Get_Page_Fault_Address();
    Debug("Page fault @%lx\n", address);

    /* Get the fault code */
    faultCode = *((faultcode_t *) &(state->errorCode));

    /* rest of your handling code here */
    Print ("Unexpected Page Fault received\n");
    Print_Fault_Info(address, faultCode);
    Dump_Interrupt_State(state);
    /* user faults just kill the process */
    if (!faultCode.userModeFault) KASSERT(0);

    /* For now, just kill the thread/process. */
    Exit(-1);
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */


/*
 * Initialize virtual memory by building page tables
 * for the kernel and physical memory.
 */
void Init_VM(struct Boot_Info *bootInfo)
{
    /*
     * Hints:
     * - Build kernel page directory and page tables
     * - Call Enable_Paging() with the kernel page directory
     * - Install an interrupt handler for interrupt 14,
     *   page fault
     * - Do not map a page at address 0; this will help trap
     *   null pointer references
     */
    /*TODO("Build initial kernel page directory and page tables");*/

    pde_t* pageDir = NULL;
    pte_t* pageTable = NULL;
    struct Page* tmpPage = NULL;
    unsigned int i = 0;
    ulong_t addr;

    /* First, we create a page directory */
    pageDir = (pde_t*)Alloc_Page();
    memset(pageDir, '\0', PAGE_SIZE);

    /* Now, I'll create the page tables BACKWARDS! MUAHAHAHAHAHA */
    for(i=0; i<(bootInfo->memSizeKB*1024)/PAGE_SIZE; i++)
    {
        addr = i*PAGE_SIZE;

        /* First, let's see if I need to allocate a new page Table */
        if(i % (PAGE_SIZE/sizeof(int)) == 0)
        {
            /* Let's allocate a new Page Table */
            pageTable = (pte_t*)Alloc_Page();
            memset(pageTable, '\0', PAGE_SIZE);
            pageDir[PAGE_DIRECTORY_INDEX(addr)].present = 0x01;
            pageDir[PAGE_DIRECTORY_INDEX(addr)].flags = VM_WRITE | VM_READ | VM_EXEC | VM_USER;
            pageDir[PAGE_DIRECTORY_INDEX(addr)].pageTableBaseAddr=PAGE_ALLIGNED_ADDR(pageTable);
        }

        /* Now, let's map a new page to its place in the Page Table */
        if(addr != 0)
        {
            tmpPage = Get_Page(addr);
            tmpPage->flags |= (PAGE_KERN | PAGE_LOCKED);
            /* | PAGE_ALLOCATED da fail en assert */
                   tmpPage->vaddr = addr;
            tmpPage->entry = &pageTable[PAGE_TABLE_INDEX(addr)];

            pageTable[PAGE_TABLE_INDEX(addr)].present = 1;
            pageTable[PAGE_TABLE_INDEX(addr)].flags = VM_WRITE | VM_READ | VM_EXEC | VM_USER;

            /* I don't know why this works
             * It should be:
             * pageTable[PAGE_TABLE_INDEX(addr)].pageBaseAddr = (uint_t)addr;
             */
            pageTable[PAGE_TABLE_INDEX(addr)].pageBaseAddr = PAGE_DIRECTORY_INDEX(addr)*1024 + PAGE_TABLE_INDEX(addr); //(uint_t)addr;
        }
    }

    /* Finally, let's enable paging and pray */
    /* Update: not enough faith, pray stronger */
    Enable_Paging(pageDir);

    /* It would be easy to blame this line for the death of my VM,
     * but to be fair it died once I added the previous line.
     * I blame forest Imps
     */
    Install_Interrupt_Handler(14, Page_Fault_Handler);

}

/**
 * Initialize paging file data structures.
 * All filesystems should be mounted before this function
 * is called, to ensure that the paging file is available.
 */
void Init_Paging(void)
{
    TODO("Initialize paging file data structures");
}

/**
 * Find a free bit of disk on the paging file for this page.
 * Interrupts must be disabled.
 * @return index of free page sized chunk of disk space in
 *   the paging file, or -1 if the paging file is full
 */
int Find_Space_On_Paging_File(void)
{
    KASSERT(!Interrupts_Enabled());
    TODO("Find free page in paging file");
}

/**
 * Free a page-sized chunk of disk space in the paging file.
 * Interrupts must be disabled.
 * @param pagefileIndex index of the chunk of disk space
 */
void Free_Space_On_Paging_File(int pagefileIndex)
{
    KASSERT(!Interrupts_Enabled());
    TODO("Free page in paging file");
}

/**
 * Write the contents of given page to the indicated block
 * of space in the paging file.
 * @param paddr a pointer to the physical memory of the page
 * @param vaddr virtual address where page is mapped in user memory
 * @param pagefileIndex the index of the page sized chunk of space
 *   in the paging file
 */
void Write_To_Paging_File(void *paddr, ulong_t vaddr, int pagefileIndex)
{
    struct Page *page = Get_Page((ulong_t) paddr);
    KASSERT(!(page->flags & PAGE_PAGEABLE)); /* Page must be locked! */
    TODO("Write page data to paging file");
}

/**
 * Read the contents of the indicated block
 * of space in the paging file into the given page.
 * @param paddr a pointer to the physical memory of the page
 * @param vaddr virtual address where page will be re-mapped in
 *   user memory
 * @param pagefileIndex the index of the page sized chunk of space
 *   in the paging file
 */
void Read_From_Paging_File(void *paddr, ulong_t vaddr, int pagefileIndex)
{
    struct Page *page = Get_Page((ulong_t) paddr);
    KASSERT(!(page->flags & PAGE_PAGEABLE)); /* Page must be locked! */
    TODO("Read page data from paging file");
}


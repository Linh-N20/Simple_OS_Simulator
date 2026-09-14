/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   int need_free_caller = 0;

   /*
    * Safely find the calling process by PID from the kernel running_list.
    * User space must NEVER pass a raw pcb_t* — we look it up ourselves.
    * This enforces the user/kernel space separation requirement.
    */
   struct pcb_t *caller = NULL;
   if (krnl->running_list != NULL) {
       struct queue_t *rl = krnl->running_list;
       int i;
       for (i = 0; i < rl->size; i++) {
           if (rl->proc[i] != NULL && rl->proc[i]->pid == pid) {
               caller = rl->proc[i];
               break;
           }
       }
   }

   /*
    * Safe fallback: if the process is not yet in running_list
    * (e.g. during its very first instruction), create a minimal
    * proxy struct pointing to the real kernel context.
    */
   if (caller == NULL) {
        printf("ERROR: cannot find caller with pid %u in running_list\n", pid);
        return -1;
   }

   switch (memop) {
   case SYSMEM_MAP_OP:
       /*
        * vmap_pgd_memset: initialise page-directory entries.
        * a2 = start virtual address, a3 = number of pages.
        */
       vmap_pgd_memset(caller, regs->a2, regs->a3);
       break;

   case SYSMEM_INC_OP:
       /*
        * inc_vma_limit: grow vm area regs->a2 by regs->a3 bytes.
        * Used by __alloc when the current vm area has no free space.
        */
       inc_vma_limit(caller, regs->a2, regs->a3);
       break;

   case SYSMEM_SWP_OP:
       /*
        * __mm_swap_page: copy one RAM frame to a SWAP frame.
        * a2 = source RAM frame number, a3 = dest SWAP frame number.
        */
       __mm_swap_page(caller, regs->a2, regs->a3);
       break;

   case SYSMEM_IO_READ:
       /*
        * Direct physical memory read.
        * a2 = physical byte address, result stored back into a3.
        * Use krnl->mram directly — no PCB needed.
        */
       if (krnl->mram == NULL) break;
       MEMPHY_read(krnl->mram, (addr_t)regs->a2, &value);
       regs->a3 = (arg_t)value;
       break;

   case SYSMEM_IO_WRITE:
       /*
        * Direct physical memory write.
        * a2 = physical byte address, a3 = byte value to write.
        * Use krnl->mram directly — no PCB needed.
        */
       if (krnl->mram == NULL) break;
       MEMPHY_write(krnl->mram, (addr_t)regs->a2, (BYTE)regs->a3);
       break;

   default:
       printf("Memop code: %d\n", memop);
       break;
   }

   if (need_free_caller)
       free(caller);

   return 0;
}



/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);
  struct mm_struct *mm = caller->own_mm;
  if (mm == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(mm, vmaid);
  int inc_sz = 0;
  if (cur_vma == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /*Attempt to increase limit to get space */
#ifdef MM64
  inc_sz = (uint32_t)(size/(int)PAGING64_PAGESZ);
  inc_sz = inc_sz + 1;
#else
  inc_sz = PAGING_PAGE_ALIGNSZ(size);
#endif
  
  int old_sbrk = cur_vma->sbrk;

  /* INCREASE THE LIMIT
   * SYSCALL 17 sys_memmap
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = size; 
#else
  regs.a3 = inc_sz; /* Request aligned size */
#endif  
  _syscall(caller->krnl, caller->pid, 17, &regs);

  /*Successful increase limit */
  mm->symrgtbl[rgid].rg_start = old_sbrk;
  mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  /* NEW FIX: Save the orphaned memory back to the free list */
  if (regs.a3 > size) {
      struct vm_rg_struct *leftover = malloc(sizeof(struct vm_rg_struct));
      leftover->rg_start = old_sbrk + size;
      leftover->rg_end = old_sbrk + regs.a3;
      leftover->rg_next = NULL;
      enlist_vm_freerg_list(mm, leftover);
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct mm_struct *mm = caller->own_mm;
  if (mm == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *rgnode = get_symrg_byid(mm, rgid);

  if (rgnode->rg_start == 0 && rgnode->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;
  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  printf("%s:%d\n", __func__, __LINE__);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
printf("%s:%d\n",__func__,__LINE__);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pgn: page number
 *@fpn: return frame number
 *@caller: caller
 *
 * If the page is already in RAM (present, not swapped) this is a fast-path
 * returning the FPN directly.  Otherwise the function performs a full
 * swap-out / swap-in cycle using the FIFO eviction policy:
 *
 *   1. Find the oldest in-RAM page (victim) from mm->fifo_pgn.
 *   2. Allocate a free SWAP frame to hold the victim.
 *   3. Copy victim RAM frame → SWAP frame  (swap-out victim).
 *   4. Mark victim PTE as swapped.
 *   5. If the target page was previously swapped, copy its SWAP frame
 *      back into the now-free victim RAM frame  (swap-in target).
 *   6. Install a fresh PTE for the target pointing to that RAM frame.
 *   7. Enqueue target in FIFO for future eviction consideration.
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = pte_get_entry(caller, pgn);

  if (!PAGING_PAGE_PRESENT(pte))
  {
    /* ---- Page is NOT currently in RAM: needs swap-in ---- */
    addr_t vicpgn;   /* victim page number (evicted from RAM)   */
    addr_t swpfpn;   /* swap frame that will hold the victim    */
    addr_t vicfpn;   /* RAM frame number of the victim page     */

    /* Step 1 – choose a victim page via FIFO */
    if (find_victim_page(mm, &vicpgn) == -1)
      return -1;

    /* Step 2 – reserve a free frame in the active swap device */
    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
      return -1;

    /* Step 3 – read the victim's current RAM frame number from its PTE */
    vicfpn = PAGING_FPN(pte_get_entry(caller, vicpgn));

    /*
     * Step 4 – swap out the victim:
     *   copy RAM[vicfpn] → SWAP[swpfpn]  via SYSCALL 17 / SYSMEM_SWP_OP
     *   which dispatches to __mm_swap_page().
     */
    struct sc_regs regs;
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;   /* source: RAM frame  */
    regs.a3 = swpfpn;   /* dest:   SWAP frame */
    _syscall(caller->krnl, caller->pid, 17, &regs);

    /* Step 5 – update victim PTE: present but swapped, offset = swpfpn */
    pte_set_swap(caller, vicpgn, 0, swpfpn);

    /*
     * Step 6 – swap in the target page (if it was previously swapped out).
     *
     * pte != 0  means the page was mapped at some point and then evicted;
     * its swap frame is recovered from PAGING_SWP(pte).
     *
     * pte == 0  means this is a freshly allocated page; the victim RAM
     * frame (vicfpn) is already zeroed / reusable — nothing to copy.
     */
    if (pte != 0) {
      addr_t tgtswpfpn = PAGING_SWP(pte);

      /* Copy SWAP[tgtswpfpn] → RAM[vicfpn]  (reverse direction swap) */
      __swap_cp_page(caller->krnl->active_mswp, tgtswpfpn,
                     caller->krnl->mram, vicfpn);

      /* Return the swap frame that was holding the target */
      MEMPHY_put_freefp(caller->krnl->active_mswp, tgtswpfpn);
    }

    /* Step 7 – install PTE for target: present, not swapped, frame = vicfpn */
    pte_set_fpn(caller, pgn, vicfpn);

    /* Step 8 – enqueue target in FIFO so it can be evicted in future */
    enlist_pgn_node(&mm->fifo_pgn, pgn);
  }

  /* Return the frame number from the (now up-to-date) PTE */
  *fpn = PAGING_FPN(pte_get_entry(caller, pgn));

  return 0;
}

/*pg_getval - read one byte from virtual address
 *@mm: memory region
 *@addr: virtual address
 *@data: output byte
 *@caller: caller
 *
 * Translates virtual address to physical address through the page table
 * (swapping in the page if necessary), then issues a MEMPHY read
 * via SYSCALL 17 / SYSMEM_IO_READ.
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Ensure the page is resident in RAM (swap in if needed) */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* Physical address = frame base + page offset */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /* Issue a MEMPHY read through the memory-map system call */
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = (addr_t)phyaddr;
  regs.a3 = (addr_t)data;
  _syscall(caller->krnl, caller->pid, 17, &regs);

  return 0;
}

/*pg_setval - write one byte to virtual address
 *@mm: memory region
 *@addr: virtual address
 *@value: byte to write
 *@caller: caller
 *
 * Translates virtual address to physical address through the page table
 * (swapping in the page if necessary), then issues a MEMPHY write
 * via SYSCALL 17 / SYSMEM_IO_WRITE.
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Ensure the page is resident in RAM (swap in if needed) */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* Physical address = frame base + page offset */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /* Issue a MEMPHY write through the memory-map system call */
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = (addr_t)phyaddr;
  regs.a3 = (addr_t)value;
  _syscall(caller->krnl, caller->pid, 17, &regs);

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  struct mm_struct *mm = caller->own_mm;
  if (mm == NULL)
    return -1;
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  if (currg == NULL)
    return -1;
//struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  /* TODO Invalid memory identify */

  pg_getval(mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
printf("%s:%d\n",__func__,__LINE__);
  int val = __read(proc, 0, source, offset, &data);

  *destination = data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct mm_struct *mm = caller->own_mm;
  if (mm == NULL) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *currg = get_symrg_byid(mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pg_setval(mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  printf("%s:%d\n", __func__, __LINE__);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

  return val;
}


/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t * caller, uint32_t size, uint32_t reg_index)
{
  /* TODO: provide OS level management
   *       and forward the request to helper
   */
//addr_t  addr;
//int val = __kmalloc(caller, -1, reg_index, size, &addr);

  /* TODO: provide OS kmem allocation validation
   */

  return 0;
}


/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /* TODO: provide OS kernel memory allocation
   *       update krnl_pgd for OS kernel level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->symrgtbl...
  //krnl->krnl_pgd ...

  return 0;

}

/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
  /* TODO: provide OS level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...

  return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
  /* TODO: provide OS level management
   *       and forward the request to helper
   */
  addr_t addr = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &addr);

  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...

  return 0;
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
  /* TODO: provide OS level management */
  /* TODO: provide OS level management */

  //struct krnl_t *krnl = caller->krnl;
  //krnl->symrgtbl...
  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...

  return 0;

}


int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  /* TODO: provide OS level management kmem
   */
  /*
   * TODO: Map kernel address range
   */
  //__read_user_mem(...)
  //__write_kernel_mem(...);

  return 0;
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  /* TODO: provide OS level management kmem
   */
  /*
   * TODO: Map kernel address range
   */
  //__read_kernel_mem(...)
  //__write_user_mem(...);

  return 1;
}


/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* TODO: provide OS memory operator for kernel memory region */
  //krnl->krnl_pgd ... or krnl->pgd ... based on kmem implementation strategy

  return 0;
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  /* TODO: provide OS memory operator for kernel memory region */
  //krnl->krnl_pgd ... or krnl->pgd ... based on kmem implementation strategy

  return 0;
}

/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* TODO: provide OS level management user memory access */
  //krnl->pgd ...

   return 0;
}


/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  /* TODO: provide OS level management user memory access */
  //krnl->pgd ...

  return 0;
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    struct mm_struct *mm = caller->own_mm;
    if (mm == NULL) {
      pthread_mutex_unlock(&mmvm_lock);
      return -1;
    }

    pte = mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!pg)
  {
    return -1;
  }
  struct pgn_t *prev = NULL;
  while (pg->pg_next)
  {
    prev = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
  if (prev != NULL)
    prev->pg_next = NULL;
  else
    mm->fifo_pgn = NULL; /* list had exactly one element */

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  if (caller == NULL || caller->own_mm == NULL)
    return -1;
  struct mm_struct *mm = caller->own_mm;

  struct vm_area_struct *cur_vma = get_vma_by_num(mm, vmaid);
  if (cur_vma == NULL)
    return -1;
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /* Use up all space, remove current node cleanly */
        if (prev == NULL) {
            /* Removing the head of the list */
            cur_vma->vm_freerg_list = rgit->rg_next;
        } else {
            /* Removing a node in the middle or end */
            prev->rg_next = rgit->rg_next;
        }
        free(rgit);
      }
      break;
    }
    else
    {
      prev = rgit;
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif

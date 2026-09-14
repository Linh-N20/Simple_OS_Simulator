/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/* Forward declaration — defined in mm64.c (MM64) or mm.c (!MM64) */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend,
                  addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg);

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  if (mm == NULL)
    return NULL;
  struct vm_area_struct *pvma = mm->mmap;
  if (pvma == NULL) return NULL;

  while (pvma != NULL && pvma->vm_id < vmaid)
  {
    pvma = pvma->vm_next;
  }
  if (pvma != NULL && pvma->vm_id == vmaid)
    return pvma;
  return NULL;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node_at_brk - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: size in bytes to grow
 *@alignedsz: page-aligned size
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  if (caller == NULL || caller->own_mm == NULL)
    return NULL;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->own_mm, vmaid);
  if (cur_vma == NULL)
    return NULL;
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL)
    return NULL;
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end   = newrg->rg_start + size;
  newrg->rg_next  = NULL;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: proposed start address
 *@vmaend: proposed end address
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  if (caller == NULL || caller->own_mm == NULL)
    return -1;
  if (vmastart >= vmaend)
    return -1;

  struct vm_area_struct *vma = caller->own_mm->mmap;
  if (vma == NULL)
    return -1;

  struct vm_area_struct *cur_area = get_vma_by_num(caller->own_mm, vmaid);
  if (cur_area == NULL)
    return -1;

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
      return -1;
    vma = vma->vm_next;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size in bytes
 *
 * Grows the vm area's sbrk by inc_sz (page-aligned), then allocates one
 * physical RAM frame per new page and installs the PTE entries so that
 * the pages are immediately accessible without a fault.
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  if (caller == NULL || caller->own_mm == NULL)
    return -1;
  /* Align the requested size up to a full page boundary */
  addr_t inc_amt    = PAGING_PAGE_ALIGNSZ(inc_sz);
  int    incnumpage = (int)(inc_amt / PAGING_PAGESZ);

  /* Obtain the new region descriptor starting at current sbrk */
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->own_mm, vmaid);

  if (cur_vma == NULL || area == NULL) {
    free(area);
    return -1;
  }

  /* Reject if the new range would overlap another vm area */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0) {
    free(area);
    return -1;
  }

  /* NEW FIX: Call the helper function you created in mm.c to map the frames safely */
  if (vm_map_ram(caller, area->rg_start, area->rg_end, area->rg_start, incnumpage, area) < 0) {
      free(area);
      return -1; 
  }

  /* Commit: push both the vm_end boundary and the sbrk pointer forward */
  cur_vma->vm_end = area->rg_end;
  cur_vma->sbrk   = area->rg_end;

  free(area);
  return 0;
}

// #endif

/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/* TODO: implement the page direactories mapping */

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}


/* Forward declaration - defined later in this file */
addr_t* get_pte(struct pcb_t *caller, addr_t pgn);

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
   addr_t *pte = get_pte(caller, pgn);
   if (pte == NULL) return -1;

    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

    return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
    addr_t *pte = get_pte(caller, pgn);
    if (pte == NULL) return -1;

    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

    SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

    return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
    return *get_pte(caller, pgn);
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	*get_pte(caller, pgn) = pte_val;	
	return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum)
{
    addr_t pgn = addr / PAGING_PAGESZ;
    for (int i = 0; i < pgnum; i++) {
      addr_t pgn_i = pgn + i;
      *get_pte(caller, pgn_i) = 0;
    }

    return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,
                       addr_t addr,
                       int pgnum,
                       struct framephy_struct *frames,
                       struct vm_rg_struct *ret_rg)
{
    struct framephy_struct *fpit = frames;
    addr_t pgn;

    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
    ret_rg->vmaid = 0;

    for (int i = 0; i < pgnum; i++)
    {
        if (fpit == NULL) return -1;

        pgn = addr / PAGING_PAGESZ + i;

        pte_set_fpn(caller, pgn, fpit->fpn);

        enlist_pgn_node(&caller->own_mm->fifo_pgn, pgn);

        fpit = fpit->fp_next;
    }

    return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */
addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    addr_t fpn;
    struct framephy_struct *head = NULL;
    struct framephy_struct *tail = NULL;

    for (int i = 0; i < req_pgnum; i++)
    {
        if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0) {
          *frm_lst = head; // giữ phần đã cấp
          return -3000;
        }

        struct framephy_struct *node = malloc(sizeof(struct framephy_struct));
        node->fpn = fpn;
        node->fp_next = NULL;

        if (head == NULL)
        {
            head = tail = node;
        }
        else
        {
            tail->fp_next = node;
            tail = node;
        }
    }

    *frm_lst = head;
    return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
//int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
   vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
    struct vm_area_struct *vma0 = calloc(1, sizeof(struct vm_area_struct));

    // page table
    mm->pgd = calloc(PAGING64_PGTBL_ENTRIES, sizeof(addr_t));
    mm->p4d = NULL;
    mm->pud = NULL;
    mm->pmd = NULL;
    mm->pt  = NULL;
    
    // init VMA
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = 0;
    vma0->sbrk = 0;
    vma0->vm_next = NULL;
    vma0->vm_mm = mm;

    struct vm_rg_struct *first_rg = init_vm_rg(0, 0);
    enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

    mm->mmap = vma0;
    mm->fifo_pgn = NULL;

    return 0;
}

addr_t* get_pte(struct pcb_t *caller, addr_t pgn)
{
    struct mm_struct *mm = caller->own_mm;
    if (mm == NULL) {
      printf("ERROR: caller %d has NULL own_mm\n", caller->pid);
      return NULL;
    }

    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    // ===== PGD → P4D =====
    if (mm->pgd[pgd_idx] == 0) {
        mm->pgd[pgd_idx] = (addr_t) calloc(PAGING64_PGTBL_ENTRIES, sizeof(addr_t));
    }
    addr_t *p4d_base = (addr_t*) mm->pgd[pgd_idx];

    // ===== P4D → PUD =====
    if (p4d_base[p4d_idx] == 0) {
        p4d_base[p4d_idx] = (addr_t) calloc(PAGING64_PGTBL_ENTRIES, sizeof(addr_t));
    }
    addr_t *pud_base = (addr_t*) p4d_base[p4d_idx];

    // ===== PUD → PMD =====
    if (pud_base[pud_idx] == 0) {
        pud_base[pud_idx] = (addr_t) calloc(PAGING64_PGTBL_ENTRIES, sizeof(addr_t));
    }
    addr_t *pmd_base = (addr_t*) pud_base[pud_idx];

    // ===== PMD → PT =====
    if (pmd_base[pmd_idx] == 0) {
        pmd_base[pmd_idx] = (addr_t) calloc(PAGING64_PGTBL_ENTRIES, sizeof(addr_t));
    }
    addr_t *pt_base = (addr_t*) pmd_base[pmd_idx];

    return &pt_base[pt_idx];
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("\n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  struct mm_struct *mm = caller->own_mm;

  /*
   * Walk the 5-level page tree starting from pgd[0] to find the
   * actual allocated sub-table pointers for display.
   * This matches the expected output format:
   *   PDG=<pgd addr> P4g=<p4d addr> PUD=<pud addr> PMD=<pmd addr>
   */
  addr_t *p4d_ptr = NULL;
  addr_t *pud_ptr = NULL;
  addr_t *pmd_ptr = NULL;

  if (mm->pgd != NULL && mm->pgd[0] != 0)
    p4d_ptr = (addr_t *)mm->pgd[0];
  if (p4d_ptr != NULL && p4d_ptr[0] != 0)
    pud_ptr = (addr_t *)p4d_ptr[0];
  if (pud_ptr != NULL && pud_ptr[0] != 0)
    pmd_ptr = (addr_t *)pud_ptr[0];

  printf("print_pgtbl:\n");
  printf(" PDG=%lx P4g=%lx PUD=%lx PMD=%lx\n",
         (unsigned long)mm->pgd,
         (unsigned long)p4d_ptr,
         (unsigned long)pud_ptr,
         (unsigned long)pmd_ptr);
  return 0;
}

#endif  //def MM64

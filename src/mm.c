/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
 
 /* NOTICE this module is deprecated in Caitoa release
  *        the structure is maintained for future 64bit-32bit
  *        backward compatible feature or PAE feature 
  */
 
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(MM64)

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

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
 * get_pd_from_address - Parse address to 5 page directory levels
 * (32-bit PAE / deprecated path)
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory levels
 * (32-bit PAE / deprecated path)
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
  printf("[ERROR] %s: This feature 32 bit mode is deprecated\n", __func__);
  return 0;
}

/*
 * pte_set_swap - Set PTE entry for a swapped-out page
 * @caller  : process
 * @pgn     : page number
 * @swptyp  : swap type (device index)
 * @swpoff  : frame offset inside the swap device
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = &krnl->mm->pgd[pgn];
	
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for a page that is resident in RAM
 * @caller : process
 * @pgn    : page number
 * @fpn    : physical frame number in MEMRAM
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  struct krnl_t *krnl = caller->krnl;
  addr_t *pte = &krnl->mm->pgd[pgn];

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

/*
 * pte_get_entry - Read the raw PTE value for a given page number
 * @caller : process
 * @pgn    : page number
 *
 * Returns the 32-bit PTE stored in mm->pgd[pgn].
 * A value of 0 means the page has never been mapped.
 */
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  struct krnl_t *krnl = caller->krnl;
  return caller->krnl->mm->pgd[pgn];
}

/*
 * pte_set_entry - Overwrite a PTE with an arbitrary raw value
 * @caller  : process
 * @pgn     : page number
 * @pte_val : new PTE value
 */
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
  struct krnl_t *krnl = caller->krnl;
  caller->krnl->mm->pgd[pgn] = pte_val;
  return 0;
}

/*
 * vmap_pgd_memset - dummy-allocate a range of pages in the page directory
 * @caller : process
 * @addr   : start virtual address (page-aligned)
 * @pgnum  : number of pages to cover
 *
 * Used by the SYSMEM_MAP_OP syscall to emulate page-directory
 * behaviour without committing real physical frames.  PTEs are
 * installed as present but with FPN = 0.
 */
int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum)
{
  addr_t pgit = PAGING_PGN(addr);
  int i;

  for (i = 0; i < pgnum; i++)
    pte_set_fpn(caller, pgit + i, 0);  /* dummy: present, fpn=0 */

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum physical frames from MRAM
 * @caller    : process
 * @req_pgnum : number of frames to allocate
 * @frm_lst   : output: linked list of allocated framephy_struct nodes
 *
 * Returns 0 on success, -1 if MRAM is exhausted.
 */
addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum,
                         struct framephy_struct **frm_lst)
{
  int i;
  struct framephy_struct *head = NULL;

  for (i = 0; i < req_pgnum; i++) {
    addr_t fpn;

    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) < 0) {
      /* RAM exhausted — caller must handle eviction */
      return -1;
    }

    struct framephy_struct *node = malloc(sizeof(struct framephy_struct));
    node->fpn     = fpn;
    node->fp_next = head;
    head = node;
  }

  *frm_lst = head;
  return 0;
}

/*
 * vmap_page_range - install PTEs for a contiguous virtual range
 * @caller  : process
 * @addr    : start virtual address (page-aligned)
 * @pgnum   : number of pages to map
 * @frames  : linked list of physical frames (one per page)
 * @ret_rg  : output: the virtual region that was mapped
 *
 * Each page starting at PAGING_PGN(addr) is wired to the next frame
 * from the frames list.  Pages are also enqueued in the FIFO eviction
 * list so they become eviction candidates.
 */
addr_t vmap_page_range(struct pcb_t *caller,
                       addr_t addr,
                       int pgnum,
                       struct framephy_struct *frames,
                       struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *fpit = frames;
  addr_t pgit = PAGING_PGN(addr);
  int i;

  ret_rg->rg_start = addr;
  ret_rg->rg_end   = addr + (addr_t)pgnum * PAGING_PAGESZ;

  for (i = 0; i < pgnum && fpit != NULL; i++) {
    pte_set_fpn(caller, pgit + i, fpit->fpn);
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgit + i);
    fpit = fpit->fp_next;
  }

  return 0;
}

/*
 * vm_map_ram - map a virtual region [astart, aend) to MRAM frames
 * @caller    : process
 * @astart    : virtual start address of the region
 * @aend      : virtual end address of the region  (unused, derived from incpgnum)
 * @mapstart  : virtual address at which mapping begins
 * @incpgnum  : number of pages to map
 * @ret_rg    : output: the mapped region descriptor
 *
 * Allocates incpgnum physical frames, then installs PTE entries starting
 * at page PAGING_PGN(mapstart).  The frame list is freed after use
 * because ownership passes to the page table.
 */

/*
 * __swap_cp_page - copy one page frame between two MEMPHY devices
 * @mpsrc  : source memphy (e.g. MRAM)
 * @srcfpn : source frame number
 * @mpdst  : destination memphy (e.g. MSWP)
 * @dstfpn : destination frame number
 *
 * Reads PAGING_PAGESZ bytes from src starting at srcfpn*PAGING_PAGESZ
 * and writes them to dst starting at dstfpn*PAGING_PAGESZ.
 * Works for both directions: RAM→SWAP and SWAP→RAM.
 */
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc = srcfpn * PAGING_PAGESZ;
  addr_t addrdst = dstfpn * PAGING_PAGESZ;

  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++) {
    BYTE data;
    MEMPHY_read(mpsrc,  addrsrc + cellidx, &data);
    MEMPHY_write(mpdst, addrdst + cellidx,  data);
  }

  return 0;
}

/*
 * init_mm - initialise a process's memory management descriptor
 * @mm     : uninitialised mm_struct to set up
 * @caller : owning process
 *
 * Allocates the flat page-table array (pgd), zeroes it, and creates the
 * first vm_area (vmaid = 0) that covers the user heap starting at
 * address 0.  The symbol table and the FIFO page list are also cleared.
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  /* Allocate and zero the flat single-level page table */
  mm->pgd = (addr_t *)malloc(PAGING_MAX_PGN * sizeof(addr_t));
  if (mm->pgd == NULL)
    return -1;
  memset(mm->pgd, 0, PAGING_MAX_PGN * sizeof(addr_t));

  /* Create the initial vm_area (vmaid = 0, heap segment) */
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  if (vma0 == NULL)
    return -1;

  vma0->vm_id          = 0;
  vma0->vm_start       = 0;
  vma0->vm_end         = 0;
  vma0->sbrk           = 0;       /* heap top starts at base */
  vma0->vm_freerg_list = NULL;
  vma0->vm_mm          = mm;
  vma0->vm_next        = NULL;

  mm->mmap    = vma0;
  mm->fifo_pgn = NULL;

  /* Zero the symbol table (variable→region mapping) */
  memset(mm->symrgtbl, 0, sizeof(mm->symrgtbl));

  return 0;
}

/*
 * init_vm_rg - allocate and initialise a vm_rg_struct
 * @rg_start : region start address
 * @rg_end   : region end address
 */
struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgn = malloc(sizeof(struct vm_rg_struct));
  if (rgn == NULL)
    return NULL;

  rgn->rg_start = rg_start;
  rgn->rg_end   = rg_end;
  rgn->rg_next  = NULL;

  return rgn;
}

/*
 * enlist_vm_rg_node - prepend a vm region node to a region list
 * @rglist : head pointer of the list (updated in-place)
 * @rgnode : node to prepend
 */
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;
  return 0;
}

/*
 * enlist_pgn_node - prepend a page number to the FIFO eviction list
 * @plist : head pointer of pgn_t list (updated in-place)
 * @pgn   : virtual page number to record
 *
 * New pages are inserted at the HEAD.  find_victim_page() walks to the
 * TAIL, so the oldest page (first inserted) is evicted first — FIFO.
 */
int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
  if (pnode == NULL)
    return -1;

  pnode->pgn     = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

/* ------------------------------------------------------------------ */
/* Debug / tracing helpers                                              */
/* ------------------------------------------------------------------ */

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp:");
  while (fp != NULL) {
    printf(" [fpn=%d]->", fp->fpn);
    fp = fp->fp_next;
  }
  printf(" NULL\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg:");
  while (rg != NULL) {
    printf(" [rg=%lu-%lu]->", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf(" NULL\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma:");
  while (vma != NULL) {
    printf(" [id=%lu start=%lu end=%lu sbrk=%lu]->",
           vma->vm_id, vma->vm_start, vma->vm_end, vma->sbrk);
    vma = vma->vm_next;
  }
  printf(" NULL\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  struct pgn_t *pg = ip;

  printf("print_list_pgn:");
  while (pg != NULL) {
    printf(" [pgn=%lu]->", pg->pgn);
    pg = pg->pg_next;
  }
  printf(" NULL\n");
  return 0;
}

/*
 * print_pgtbl - dump active PTE entries in range [start, end)
 * @caller : process
 * @start  : first page number to show
 * @end    : last page number to show (pass -1 for the full table)
 */

 int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  addr_t i;

  if (end == (addr_t)-1)
    end = PAGING_MAX_PGN;

  printf("print_pgtbl [page %llu .. %llu]:\n", (unsigned long long)start, (unsigned long long)end - 1);

  for (i = start; i < end; i++) {
    addr_t pte = caller->krnl->mm->pgd[i]; // Use addr_t for the PTE entry

    if (pte == 0)
      continue;

    if (PAGING_PAGE_PRESENT(pte) && !PAGING_PAGE_SWAPPED(pte)) { 
      printf("  page %4llu -> frame %4llu  (RAM)\n",
             (unsigned long long)i, (unsigned long long)PAGING_FPN(pte));
    } else {
      printf("  page %4llu -> swpoff %4llu  (SWAP)\n",
             (unsigned long long)i, (unsigned long long)PAGING_SWP(pte));
    }
  }

  return 0;
}

/* Move this to the VERY BOTTOM of src/mm.c, after the final #endif */

addr_t vm_map_range(struct pcb_t *caller, addr_t astart, addr_t aend,
                  addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;

  /* Step 1: allocate physical frames */
  if (alloc_pages_range(caller, incpgnum, &frm_lst) < 0)
    return -1;

  /* Step 2: wire pages to frames and record the virtual region */
  if (vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg) < 0) {
    /* Rollback: Release frames if mapping fails */
    struct framephy_struct *fp = frm_lst;
    while (fp != NULL) {
      struct framephy_struct *nxt = fp->fp_next;
      MEMPHY_put_freefp(caller->krnl->mram, fp->fpn);
      free(fp);
      fp = nxt;
    }
    return -1;
  }

  /* Step 3: Free the helper list (PTEs now own the frame numbers) */
  struct framephy_struct *fp = frm_lst;
  while (fp != NULL) {
    struct framephy_struct *nxt = fp->fp_next;
    free(fp);
    fp = nxt;
  }

  return 0;
}

int swap_out_victim(struct pcb_t *caller, addr_t *ret_fpn)
{
    struct pgn_t *pg = caller->krnl->mm->fifo_pgn;
    struct pgn_t *prev = NULL;

    if (pg == NULL) return -1;

    // tìm trang cũ nhất (FIFO = node cuối)
    while (pg->pg_next != NULL) {
        prev = pg;
        pg = pg->pg_next;
    }

    addr_t victim_pgn = pg->pgn;
    uint32_t pte = pte_get_entry(caller, victim_pgn);
    if (!PAGING_PAGE_PRESENT(pte) || PAGING_PAGE_SWAPPED(pte))
      return -1;
    addr_t victim_fpn = PAGING_FPN(pte);

    addr_t swpfpn;
    if (MEMPHY_get_freefp(caller->krnl->mswp[0], &swpfpn) < 0)
        return -1;

    // copy RAM → SWAP
    __swap_cp_page(
        caller->krnl->mram, victim_fpn,
        caller->krnl->mswp[0], swpfpn
    );

    // update PTE: đánh dấu swapped
    pte_set_swap(caller, victim_pgn, 0, swpfpn);

    // remove khỏi FIFO list
    if (prev == NULL)
        caller->krnl->mm->fifo_pgn = NULL;
    else
        prev->pg_next = NULL;

    free(pg);

    // trả lại frame để dùng tiếp
    *ret_fpn = victim_fpn;

    return 0;
}

int is_in_fifo(struct pgn_t *head, addr_t pgn)
{
    while (head != NULL) {
        if (head->pgn == pgn)
            return 1;
        head = head->pg_next;
    }
    return 0;
}

int __read(struct pcb_t *caller, addr_t addr, BYTE *data)
{
    if (data == NULL) return -1;

    addr_t pgn = PAGING_PGN(addr);
    addr_t offset = PAGING_OFF(addr);

    uint32_t pte = pte_get_entry(caller, pgn);
    if (pte == 0) return -1;

    // nếu page nằm trong swap → swap vào RAM
    if (PAGING_PAGE_SWAPPED(pte)) {
        addr_t swpoff = PAGING_SWP(pte);
        addr_t newfpn;

        // nếu RAM full → swap-out
        if (MEMPHY_get_freefp(caller->krnl->mram, &newfpn) < 0) {
            if (swap_out_victim(caller, &newfpn) < 0)
                return -1;
        }

        __swap_cp_page(
            caller->krnl->mswp[0], swpoff,
            caller->krnl->mram, newfpn
        );

        pte_set_fpn(caller, pgn, newfpn);
        if (!is_in_fifo(caller->krnl->mm->fifo_pgn, pgn))
          enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
        pte = pte_get_entry(caller, pgn);
    }

    addr_t phyaddr = PAGING_FPN(pte) * PAGING_PAGESZ + offset;

    MEMPHY_read(caller->krnl->mram, phyaddr, data);

    return 0;
}

int __write(struct pcb_t *caller, addr_t addr, BYTE data)
{
    addr_t pgn = PAGING_PGN(addr);
    addr_t offset = PAGING_OFF(addr);

    uint32_t pte = pte_get_entry(caller, pgn);
    if (pte == 0) return -1;

    // nếu page nằm trong swap → swap vào RAM
    if (PAGING_PAGE_SWAPPED(pte)) {
        addr_t swpoff = PAGING_SWP(pte);
        addr_t newfpn;

        // nếu RAM full → swap-out
        if (MEMPHY_get_freefp(caller->krnl->mram, &newfpn) < 0) {
            if (swap_out_victim(caller, &newfpn) < 0)
                return -1;
        }

        __swap_cp_page(
            caller->krnl->mswp[0], swpoff,
            caller->krnl->mram, newfpn
        );

        pte_set_fpn(caller, pgn, newfpn);
        if (!is_in_fifo(caller->krnl->mm->fifo_pgn, pgn))
          enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
        pte = pte_get_entry(caller, pgn);
    }

    addr_t phyaddr = PAGING_FPN(pte) * PAGING_PAGESZ + offset;

    MEMPHY_write(caller->krnl->mram, phyaddr, data);

    // set dirty bit
    uint32_t newpte = pte_get_entry(caller, pgn);
    SETBIT(newpte, PAGING_PTE_DIRTY_MASK);
    pte_set_entry(caller, pgn, newpte);

    return 0;
}
#endif /* !defined(MM64) */

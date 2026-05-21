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


/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
//struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
#ifdef MM64	
  struct mm_struct *mm = caller->krnl->mm;

  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

  mm->pgd[pgd] = pgd;
  mm->p4d[p4d] = p4d;
  mm->pud[pud] = pud;
  mm->pmd[pmd] = pmd;

  pte = &mm->pt[pt];
  
#else
  pte = &caller->krnl->mm->pgd[pgn];
#endif
	tlb_invalidate(mm, pgn);

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
  struct mm_struct *mm = caller->krnl->mm;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  mm->pgd[pgd] = pgd;
  mm->p4d[p4d] = p4d;
  mm->pud[pud] = pud;
  mm->pmd[pmd] = pmd;
  
  pte = &mm->pt[pt];
#else
  pte = &caller->krnl->mm->pgd[pgn];
#endif
  tlb_invalidate(mm, pgn);

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}

void tlb_invalidate(struct mm_struct *mm, addr_t vpn)
{
    for (int i = 0; i < TLB_SIZE; i++) {
        if (mm->tlb[i].valid &&
            mm->tlb[i].vpn == vpn)
        {
            mm->tlb[i].valid = 0;
        }
    }
}

/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
    struct mm_struct *mm = caller->krnl->mm;

    uint32_t pte = 0;

    addr_t pgd=0;
    addr_t p4d=0;
    addr_t pud=0;
    addr_t pmd=0;
    addr_t pt=0;

    /* TLB lookup */
    for (int i = 0; i < TLB_SIZE; i++) {

        if (mm->tlb[i].valid &&
            mm->tlb[i].vpn == pgn){
            SETBIT(mm->tlb[i].pte,
                  PAGING_PTE_REFERENCED_MASK);

            get_pd_from_pagenum(
                pgn,
                &pgd,
                &p4d,
                &pud,
                &pmd,
                &pt
            );

            mm->pt[pt] = mm->tlb[i].pte;

            return mm->tlb[i].pte;
        }
    }

    /* Page walk */
    get_pd_from_pagenum(
        pgn,
        &pgd,
        &p4d,
        &pud,
        &pmd,
        &pt
    );
    pte = mm->pt[pt];
    
    if (!PAGING_PAGE_PRESENT(pte))
    return 0;

    /* Set referenced bit */
    SETBIT(pte, PAGING_PTE_REFERENCED_MASK);

    mm->pt[pt] = pte;

    /* Insert into TLB */
    int victim = mm->tlb_replace;

    mm->tlb[victim].valid = 1;
    mm->tlb[victim].vpn   = pgn;
    mm->tlb[victim].pte   = pte;

    mm->tlb_replace =
        (victim + 1) % TLB_SIZE;

    return pte;

}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	struct mm_struct *mm = caller->krnl->mm;
  uint32_t pte = 0;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t	pt=0;
	
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
	tlb_invalidate(mm, pgn);
  mm->pt[pt] = pte_val;
	return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  int pgit = 0;
  uint64_t pattern = 0xdeadbeef;

  for (pgit = 0; pgit < pgnum; pgit++){
    addr_t pgn = PAGING_PGN(addr) + pgit;
    pte_set_entry(caller, pgn, pattern);
  }

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  struct framephy_struct *fpit = frames;
  int pgit = 0;
  addr_t pgn;

  if (caller == NULL || frames == NULL || pgnum <= 0){
    return 0;
  }

  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ;
  ret_rg->vmaid = 0;

  while(pgit < pgnum && fpit != NULL) {
    pgn = PAGING_PGN(addr) + pgit;
    pte_set_fpn(caller, pgn, fpit->fpn);
    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
    pgit++; 
    fpit = fpit->fp_next;
  }

  return addr;
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
  int pgit = 0;
  struct framephy_struct *newfp_str = NULL;

  struct framephy_struct *head = NULL;
  struct framephy_struct *tail = NULL;
  // TODO: allocate the page 
  //caller-> ...
  //frm_lst-> ...



  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    // TODO: allocate the page 
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
       newfp_str =
                (struct framephy_struct*)
                malloc(sizeof(struct framephy_struct));

            if (newfp_str == NULL)
                return -1;

            newfp_str->fpn = fpn;
            newfp_str->fp_next = NULL;

            if (head == NULL)
            {
                head = newfp_str;
                tail = newfp_str;
            }
            else
            {
                tail->fp_next = newfp_str;
                tail = newfp_str;
            }
        }
        else
        {
            /* Not enough physical frames */
            return -3000;
        }
  }

  *frm_lst = head;

  /* End TODO */

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
  addr_t ret_alloc;

  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  if (ret_alloc == -3000)
    return -1; /* Out of physical memory */

  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/*
 * vm_map_range - map a user VM range to RAM (delegates to vm_map_ram)
 */
addr_t vm_map_range(struct pcb_t *caller, addr_t astart, addr_t aend,
                    addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  return vm_map_ram(caller, astart, aend, mapstart, incpgnum, ret_rg);
}

/*
 * vm_map_kernel - map a kernel VM range to RAM and wire krnl_pgd directly.
 * The kernel page table is treated as a flat array indexed by page number.
 */
addr_t vm_map_kernel(struct pcb_t *caller, addr_t astart, addr_t aend,
                     addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  if (!caller || !caller->krnl || incpgnum <= 0) return -1;

  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000) return -1;
  if (ret_alloc == -3000) return -1;

  struct krnl_t *krnl = caller->krnl;
  struct framephy_struct *fpit = frm_lst;
  int pgit = 0;
  addr_t pgn;

  ret_rg->rg_start = mapstart;
  ret_rg->rg_end   = mapstart + (addr_t)incpgnum * PAGING_PAGESZ;
  ret_rg->vmaid    = -1; /* kernel space marker */

  while (pgit < incpgnum && fpit != NULL) {
    pgn = PAGING_PGN(mapstart) + pgit;

    uint32_t pte = 0;
    SETBIT(pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(pte, fpit->fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

    krnl->krnl_pgd[pgn] = pte;
    krnl->krnl_pt[pgn]  = pte;

    pgit++;
    fpit = fpit->fp_next;
  }

  /* Release frame-list nodes (frames themselves are now wired) */
  while (frm_lst) {
    struct framephy_struct *tmp = frm_lst->fp_next;
    free(frm_lst);
    frm_lst = tmp;
  }

  return mapstart;
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
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  /* TODO init page table directory */
  mm->pgd = (addr_t *) calloc(512, sizeof(addr_t));
  mm->p4d = (addr_t *) calloc(512, sizeof(addr_t));
  mm->pud = (addr_t *) calloc(512, sizeof(addr_t));
  mm->pmd = (addr_t *) calloc(512, sizeof(addr_t));
  mm->pt = (addr_t *) calloc(512, sizeof(addr_t));

  
  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  vma0->vm_next = NULL;

  /* Point vma owner backward */
  vma0->vm_mm = mm; 

  /* TODO: update mmap */
  mm->fifo_pgn = NULL;
  mm->mmap = vma0;
  mm->kcpooltbl = NULL;
  for(int i = 0; i<PAGING_MAX_SYMTBL_SZ; i++){
    mm->symrgtbl[i].rg_start = 0;
    mm->symrgtbl[i].rg_end = 0;
    mm->symrgtbl[i].vmaid = -1;
  }
  for (int i = 0; i < TLB_SIZE; i++)
  {
      mm->tlb[i].valid = 0;
      mm->tlb[i].vpn   = 0;
      mm->tlb[i].pte   = 0;
  }

  mm->tlb_replace = 0;
  return 0;
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
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  addr_t pgn_start, pgn_end;
  addr_t pgit;
  struct krnl_t *krnl = caller->krnl;

  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;

  get_pd_from_address(start, &pgd, &p4d, &pud, &pmd, &pt);

  /* TODO traverse the page map and dump the page directory entries */
  struct mm_struct *mm =
        caller->krnl->mm;

    pgn_start = PAGING_PGN(start);

    pgn_end = PAGING_PGN(end);

    for (pgit = pgn_start; pgit <= pgn_end; pgit++){
        get_pd_from_pagenum(pgit, &pgd, &p4d, &pud, &pmd, &pt);

        printf("PGN[%lu] "
            "PGD[%lu] "
            "P4D[%lu] "
            "PUD[%lu] "
            "PMD[%lu] "
            "PT[%lu] "
            "PTE=0x%08x\n",

            pgit,
            pgd,
            p4d,
            pud,
            pmd,
            pt,

            (uint32_t) mm->pt[pt]
        );
    }
  return 0;
}

#endif  //def MM64

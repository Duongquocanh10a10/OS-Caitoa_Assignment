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
  if(!caller || !caller->krnl || !caller->krnl->mm || !alloc_addr) return -1;
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) return -1;
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  int inc_sz=0;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* get_free_vmrg_area FAILED — extend VMA then retry */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = size;
  if (_syscall(caller->krnl, caller->pid, 17, &regs) != 0) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* inc_vma_limit added new space to the free list — get it from there */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) != 0) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
  caller->krnl->mm->symrgtbl[rgid].rg_end   = rgnode.rg_end;
  *alloc_addr = rgnode.rg_start;

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
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) return -1;
  pthread_mutex_lock(&mmvm_lock);
  
  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);
  if(!rgnode){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (rgnode->rg_start == 0 && rgnode->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  //addr_t start_addr = rgnode->rg_start;
  //addr_t end_addr = rgnode->rg_end;

  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  if(!freerg_node){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

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
  /* TODO dump IO content (if needed) */
  printf("Alloc log: Alloc: %ld bytes, reg_index: %d, addr: 0x%08lx, PID: %d\n", size, reg_index, addr, proc->pid);
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
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("Freed: reg_index: %d, PID: %d \n", reg_index, proc->pid);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  if(!mm || !caller || !fpn){
    return -1;
  }
  if(pgn<0 || pgn>=PAGING_MAX_PGN){
    return -1;
  }
  uint32_t pte = pte_get_entry(caller, pgn);
  if (PAGING_PAGE_PRESENT(pte) && !(pte & PAGING_PTE_SWAPPED_MASK)) {
    *fpn = PAGING_FPN(pte);
    return 0;
  }
  if (!PAGING_PAGE_PRESENT(pte) || (pte & PAGING_PTE_SWAPPED_MASK))
  { /* Page is not online, make it actively living */
    addr_t vicpgn, swpfpn;
    addr_t vicfpn;
    addr_t vicpte;
    struct sc_regs regs;

    /* TODO Initialize the target frame storing our variable */
    int swpd = (pte & PAGING_PTE_SWAPPED_MASK);
    addr_t tgtfpn;
    if(MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) == 0){
      if(swpd){
        addr_t srcswpfpn = PAGING_SWP(pte);

        regs.a1 = SYSMEM_SWP_OP;
        regs.a2 = srcswpfpn; 
        regs.a3 = tgtfpn;    
        _syscall(caller->krnl, caller->pid, 17, &regs);
        MEMPHY_put_freefp(caller->krnl->active_mswp, srcswpfpn);
      }

      pte_set_fpn(caller, pgn, tgtfpn);
#ifdef MM64
      SETBIT(mm->pt[pgn & 0x1FF], PAGING_PTE_RESERVE_MASK);
#else
      SETBIT(mm->pgd[pgn], PAGING_PTE_RESERVE_MASK);
#endif
      enlist_pgn_node(&mm->fifo_pgn, pgn);

      pte = pte_get_entry(caller, pgn);
      if (!PAGING_PAGE_PRESENT(pte) || (pte & PAGING_PTE_SWAPPED_MASK)) return -1;

      *fpn = PAGING_FPN(pte_get_entry(caller, pgn));
      return 0;
    }
    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if (find_victim_page(mm, &vicpgn) == -1)
    {
      return -1;
    }
    vicpte = pte_get_entry(caller, vicpgn);
    vicfpn = PAGING_FPN(vicpte);

    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
    {
      return -1;
    }

    /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/

    /* TODO copy victim frame to swap 
     * SWP(vicfpn <--> swpfpn)
     * SYSCALL 1 sys_memmap
     */
    regs.a1 = SYSMEM_SWP_OP;
    regs.a2 = vicfpn;
    regs.a3 = swpfpn;
    _syscall(caller->krnl, caller->pid, 17, &regs);

    int vic_swap = (vicpte & PAGING_PTE_SWAPPED_MASK);
    addr_t vic_swpfpn = vic_swap ? PAGING_SWP(vicpte) : 0;
    /* Update page table */
    //pte_set_swap(...);
    pte_set_swap(caller, vicpgn, 0, swpfpn);
#ifdef MM64
    CLRBIT(mm->pt[vicpgn & 0x1FF], PAGING_PTE_PRESENT_MASK);
#else
    CLRBIT(mm->pgd[vicpgn], PAGING_PTE_PRESENT_MASK);
#endif
    if(vic_swap){
      MEMPHY_put_freefp(caller->krnl->active_mswp, vic_swpfpn);
    }

    tgtfpn = vicfpn;
    if(swpd){
      addr_t srcswpfpn = PAGING_SWP(pte);

      regs.a1 = SYSMEM_SWP_OP;
      regs.a2 = srcswpfpn; 
      regs.a3 = tgtfpn;    
      _syscall(caller->krnl, caller->pid, 17, &regs);

      MEMPHY_put_freefp(caller->krnl->active_mswp, srcswpfpn);
    }
    /* Update its online status of the target page */
    //pte_set_fpn(...);
    pte_set_fpn(caller, pgn, vicfpn);
#ifdef MM64
    SETBIT(mm->pt[pgn & 0x1FF], PAGING_PTE_RESERVE_MASK);
    CLRBIT(mm->pt[pgn & 0x1FF], PAGING_PTE_DIRTY_MASK);
#else
    SETBIT(mm->pgd[pgn], PAGING_PTE_RESERVE_MASK);
    CLRBIT(mm->pgd[pgn], PAGING_PTE_DIRTY_MASK);
#endif
    enlist_pgn_node(&mm->fifo_pgn, pgn);
    pte = pte_get_entry(caller, pgn);
    if(!PAGING_PAGE_PRESENT(pte) || (pte & PAGING_PTE_SWAPPED_MASK)) return -1;
  }
  *fpn = PAGING_FPN(pte);
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  if(!mm || ! caller || !data) return -1;
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#ifdef MM64
  SETBIT(mm->pt[pgn & 0x1FF], PAGING_PTE_RESERVE_MASK);
#else
  SETBIT(mm->pgd[pgn], PAGING_PTE_RESERVE_MASK);
#endif
  /* TODO
   *  MEMPHY_read(caller->krnl->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */

  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  _syscall(caller->krnl, caller->pid, 17, &regs);
  *data = (BYTE)regs.a3;
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */


  /* TODO 
   *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
   *  MEMPHY WRITE with SYSMEM_IO_WRITE 
   * SYSCALL 17 sys_memmap
   */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = value;
  _syscall(caller->krnl, caller->pid, 17, &regs);
#ifdef MM64
  {
    addr_t pgd_i=0, p4d_i=0, pud_i=0, pmd_i=0, pt_i=0;
    get_pd_from_pagenum(pgn, &pgd_i, &p4d_i, &pud_i, &pmd_i, &pt_i);
    SETBIT(mm->pt[pt_i], PAGING_PTE_RESERVE_MASK);
    SETBIT(mm->pt[pt_i], PAGING_PTE_DIRTY_MASK);
  }
#else
  SETBIT(mm->pgd[pgn], PAGING_PTE_RESERVE_MASK);
  SETBIT(mm->pgd[pgn], PAGING_PTE_DIRTY_MASK);
#endif
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
  if(!caller || !caller->krnl || !caller->krnl->mm){
    return -1;
  }
  if(rgid<0 || rgid>= PAGING_MAX_SYMTBL_SZ){
    return -1;
  }
  if(!data){
    return -1;
  }
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  /* TODO Invalid memory identify */
  if(!cur_vma){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if(!currg || currg->rg_start >= currg->rg_end){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if(offset >= currg->rg_end - currg->rg_start){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  
  if(pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller)!=0){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  pthread_mutex_unlock(&mmvm_lock);
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
  int val = __read(proc, 0, source, offset, &data);
  if(val == -1) return -1;
  *destination = data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */

  pthread_mutex_lock(&mmvm_lock);

  struct vm_rg_struct *rg = get_symrg_byid(proc->krnl->mm, source);
  if(rg && (rg->rg_start + offset < rg->rg_end)){
    addr_t vaddr = rg->rg_start + offset;
    int pgn = PAGING_PGN(vaddr);
    int off = PAGING_OFFST(vaddr);
    uint32_t pte = pte_get_entry(proc, pgn);
    char premask = (pte & PAGING_PTE_PRESENT_MASK)? 'P': '-';
    char dirmask = (pte & PAGING_PTE_DIRTY_MASK)? 'D': '-';
    char resmask = (pte & PAGING_PTE_RESERVE_MASK)? 'R': '-';
    printf("IODump: PID: %u. ", proc->pid);
    printf("Write rg=%u, off=%u, vaddrr=0x%05lx, pgn=%d, offpg=%d val=0x%02x, ", source, (unsigned)offset, vaddr, pgn, off, data);
    printf("P:%c, D:%c, R:%c\n", premask, dirmask, resmask);
  }
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  pthread_mutex_unlock(&mmvm_lock);
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
  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (offset >= (currg->rg_end - currg->rg_start)){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if(pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller) !=0){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

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
  /* TODO dump IO content (if needed) */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *rg = get_symrg_byid(proc->krnl->mm, destination);
  if(rg && (rg->rg_start + offset < rg->rg_end)){
    addr_t vaddr = rg->rg_start + offset;
    int pgn = PAGING_PGN(vaddr);
    int off = PAGING_OFFST(vaddr);
    uint32_t pte = pte_get_entry(proc, pgn);
    char premask = (pte & PAGING_PTE_PRESENT_MASK)? 'P': '-';
    char dirmask = (pte & PAGING_PTE_DIRTY_MASK)? 'D': '-';
    char resmask = (pte & PAGING_PTE_RESERVE_MASK)? 'R': '-';
    printf("IODump: PID: %u. ", proc->pid);
    printf("Write rg=%u, off=%u, vaddrr=0x%05lx, pgn=%d, offpg=%d val=0x%02x, ", 
      destination, (unsigned)offset, vaddr, pgn, off, data);
    printf("P:%c, D:%c, R:%c\n", premask, dirmask, resmask);
    
  }
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  pthread_mutex_unlock(&mmvm_lock);
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

  /* TODO: provide OS kmem allocation validation
   */
  addr_t  addr;
  if(!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if(size==0) return -1;
  if(reg_index >= PAGING_MAX_SYMTBL_SZ) return -1;

  addr_t val = __kmalloc(caller, -1, reg_index, size, &addr);
  if(val < 0) return -1;
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
  addr_t err = -1;
  if(!caller || !caller->krnl || !caller->krnl->mm) return err;
  if(!alloc_addr) return err;
  if(size==0) return err;
  if(rgid <0 || rgid >= PAGING_MAX_SYMTBL_SZ) return err;

  struct krnl_t *krnl = caller->krnl;
  struct framephy_struct *frm_lst = NULL;
  struct vm_rg_struct ret_rg;
  pthread_mutex_lock(&mmvm_lock);
  addr_t addr = krnl->mm->mmap->sbrk;
  addr_t aligned_sz = PAGING_PAGE_ALIGNSZ(size);
  int incpgnum= (int)aligned_sz/PAGING_PAGESZ;

  if(vm_map_kernel(caller, addr, addr + aligned_sz, addr, incpgnum, &ret_rg) != 0){
    struct framephy_struct *fp = frm_lst;
    while(fp){
      struct framephy_struct *n = fp->fp_next;
      MEMPHY_put_freefp(krnl->mram, fp->fpn);
      free(fp);
      fp = n;
    }
    pthread_mutex_unlock(&mmvm_lock);
    return err;
  }
  *alloc_addr = ret_rg.rg_start;
  krnl->mm->symrgtbl[rgid].rg_start = *alloc_addr;
  krnl->mm->symrgtbl[rgid].rg_end = *alloc_addr + size;
  krnl->mm->symrgtbl[rgid].rg_next = NULL;
  krnl->mm->mmap->sbrk = addr + aligned_sz;
  //krnl->krnl_pgd ... in vm_map_kernel
  pthread_mutex_unlock(&mmvm_lock);
  return *alloc_addr;

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

  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (size == 0 || align == 0) return -1;
  struct krnl_t *krnl = caller->krnl;
  //may need changes later
  #ifndef MAX_POOLSZ
  #define MAX_POOLSZ 16
  #endif
  
  if (cache_pool_id >= MAX_POOLSZ) return -1;
  pthread_mutex_lock(&mmvm_lock);

  if (krnl->mm->kcpooltbl == NULL){
    krnl->mm->kcpooltbl = calloc(MAX_POOLSZ, sizeof(struct kcache_pool_struct));
    if (!krnl->mm->kcpooltbl) return -1;
  }
  /* prevent accidental overwrite of an active pool */
  if(krnl->mm->kcpooltbl[cache_pool_id].size != 0){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  uint32_t slotsz = ((size + align - 1) / align) * align;
  krnl->mm->kcpooltbl[cache_pool_id].size = (int)slotsz;
  krnl->mm->kcpooltbl[cache_pool_id].align = (int)align;
  krnl->mm->kcpooltbl[cache_pool_id].storage = 0;

  pthread_mutex_unlock(&mmvm_lock);
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
  if (!proc || !proc->krnl) return -1;
  addr_t addr = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &addr);

  //krnl->kcpooltbl...
  //krnl->krnl_pgd ...
  
  if (reg_index >= PAGING_MAX_SYMTBL_SZ) return -1;
  if (cache_pool_id >= MAX_POOLSZ) return -1;
  if(addr == (addr_t)-1) return -1;
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
  addr_t err = -1;
  if(!caller || !caller->krnl || !caller->krnl->mm) return err;
  if(!alloc_addr) return err;
  struct krnl_t *krnl = caller->krnl;

  if(rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) return err;
  if(cache_pool_id < 0 || cache_pool_id >= MAX_POOLSZ) return err;

  if(!krnl->mm->kcpooltbl) return err;
  if(krnl->mm->kcpooltbl[cache_pool_id].size <= 0) return err;

  uint32_t slotsz = (uint32_t)krnl->mm->kcpooltbl[cache_pool_id].size;

  if((int64_t)__kmalloc(caller, -1, rgid, (addr_t)slotsz, alloc_addr)<0) return err;

  if(krnl->mm->kcpooltbl[cache_pool_id].storage == 0){
    krnl->mm->kcpooltbl[cache_pool_id].storage = *alloc_addr;
  }

  return *alloc_addr;

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
  uint32_t i;
  BYTE data;

  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (size == 0) return 0;
  if (source >= PAGING_MAX_SYMTBL_SZ || destination >=PAGING_MAX_SYMTBL_SZ) return -1;

  for (i = 0; i < size; i++) {
    if (__read_user_mem(caller, 0, (int)source, (addr_t)(offset + i),&data) != 0) return -1;
    if (__write_kernel_mem(caller, -1, (int)destination, (addr_t)(offset+ i), data) != 0) return -1;
  }

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

  uint32_t i;
  BYTE data;

  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (size == 0) return 0;
  if (source >= PAGING_MAX_SYMTBL_SZ || destination >=PAGING_MAX_SYMTBL_SZ) return -1;

  for (i = 0; i < size; i++) {
    if (__read_kernel_mem(caller, -1, (int)source, (addr_t)(offset + i),&data) != 0) return -1;
    if (__write_user_mem(caller, 0, (int)destination, (addr_t)(offset +i), data) != 0) return -1;
  }

  return 0;

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
  struct vm_rg_struct *currg;
  addr_t vaddr;
  int pgn, off;
  uint32_t pte;
  int fpn, phyaddr;
  struct sc_regs regs;

  if (!caller || !caller->krnl || !caller->krnl->mm || !data) return -1;
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) return -1;

  currg = &caller->krnl->mm->symrgtbl[rgid];
  if (currg->rg_start >= currg->rg_end) return -1;
  if (offset >= (currg->rg_end - currg->rg_start)) return -1;

  vaddr = currg->rg_start + offset;
  pgn = (int)PAGING_PGN(vaddr);
  off = (int)PAGING_OFFST(vaddr);

  pte = caller->krnl->krnl_pgd[pgn];
  if (!PAGING_PAGE_PRESENT(pte) || (pte & PAGING_PTE_SWAPPED_MASK))
return -1;

  fpn = (int)PAGING_FPN(pte);
  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  _syscall(caller->krnl, caller->pid, 17, &regs);
  *data = (BYTE)regs.a3;

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
  struct vm_rg_struct *currg;
  addr_t vaddr;
  int pgn, off;
  uint32_t pte;
  int fpn, phyaddr;
  struct sc_regs regs;

  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) return -1;

  currg = &caller->krnl->mm->symrgtbl[rgid];
  if (currg->rg_start >= currg->rg_end) return -1;
  if (offset >= (currg->rg_end - currg->rg_start)) return -1;

  vaddr = currg->rg_start + offset;
  pgn = (int)PAGING_PGN(vaddr);
  off = (int)PAGING_OFFST(vaddr);

  pte = caller->krnl->krnl_pgd[pgn];
  if (!PAGING_PAGE_PRESENT(pte) || (pte & PAGING_PTE_SWAPPED_MASK))
return -1;

  fpn = (int)PAGING_FPN(pte);
  phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = value;
  _syscall(caller->krnl, caller->pid, 17, &regs);

  SETBIT(caller->krnl->krnl_pgd[pgn], PAGING_PTE_RESERVE_MASK);
  SETBIT(caller->krnl->krnl_pgd[pgn], PAGING_PTE_DIRTY_MASK);

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
  return __read(caller, vmaid, rgid, offset, data);
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
  return __write(caller, vmaid, rgid, offset, value);
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
    pte = caller->krnl->mm->pgd[pagenum];

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
//helper
static void rotate_head_to_tail(struct pgn_t **head){
  struct pgn_t *node;
  struct pgn_t *tail;

  if (!head) return;

  node = *head;
  *head = node->pg_next;
  node->pg_next = NULL;

  tail = *head;
  while (tail->pg_next != NULL) tail = tail->pg_next;
  tail->pg_next = node;
}
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!mm || !retpgn || !(mm->fifo_pgn)){
    return -1;
  }
  struct pgn_t *pg = mm->fifo_pgn;
  int count = 0;
  while(pg != NULL){
    count++;
    pg = pg->pg_next;
  }
  uint32_t pte;
  addr_t pgn;
  for(int i =0; i < count; i++){
    pgn = mm->fifo_pgn->pgn;
    pte = mm->pgd[pgn];
    int r = (pte & PAGING_PTE_RESERVE_MASK) != 0;
    int d = (pte & PAGING_PTE_DIRTY_MASK) != 0;

    if(!r && !d){
      struct pgn_t *victim = mm->fifo_pgn;
      mm->fifo_pgn = victim->pg_next;
      *retpgn = victim->pgn;
      free(victim);
      return 0;
    }
    rotate_head_to_tail(&mm->fifo_pgn);
  }
  for(int i = 0; i < count; i++){
    pgn = mm->fifo_pgn->pgn;
    pte = mm->pgd[pgn];
    int r = (pte & PAGING_PTE_RESERVE_MASK) != 0;
    int d = (pte & PAGING_PTE_DIRTY_MASK) != 0;

    if(!r && d){
      struct pgn_t *victim = mm->fifo_pgn;
      mm->fifo_pgn = victim->pg_next;
      *retpgn = victim->pgn;
      free(victim);
      return 0;
    }
    if(r){
      CLRBIT(mm->pgd[pgn], PAGING_PTE_RESERVE_MASK);
    }
    rotate_head_to_tail(&mm->fifo_pgn);
  }
  for(int i = 0; i < count; i++){
    pgn = mm->fifo_pgn->pgn;
    pte = mm->pgd[pgn];
    int d = (pte & PAGING_PTE_DIRTY_MASK) != 0;
    if(!d){
      struct pgn_t *victim = mm->fifo_pgn;
      mm->fifo_pgn = victim->pg_next;
      *retpgn = victim->pgn;
      free(victim);
      return 0;
    }
    rotate_head_to_tail(&mm->fifo_pgn);
  }
  struct pgn_t *victim = mm->fifo_pgn;
  if (!victim) return -1;
  mm->fifo_pgn = victim->pg_next;
  *retpgn = victim->pgn;
  free(victim);
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
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

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
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif

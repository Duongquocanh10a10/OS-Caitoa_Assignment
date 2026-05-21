/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#ifndef OSMM_H
#define OSMM_H

#include "os-cfg.h"
#include "bitops.h"
#include <stdint.h>

/* Address type: 64-bit in MM64 mode, 32-bit otherwise */
#ifdef MM64
typedef uint64_t addr_t;
#else
typedef uint32_t addr_t;
#endif

typedef unsigned char BYTE;

/* Max number of swap devices */
#define PAGING_MAX_MMSWP 4

/* Symbol table size (max variables per process) */
#define PAGING_MAX_SYMTBL_SZ 30

/* TLB size */
#define TLB_SIZE 16

/* TLB entry */
struct tlb_entry_t {
    int valid;
    addr_t vpn;
    uint32_t pte;
};

/* Physical frame descriptor */
struct framephy_struct {
    int fpn;
    struct framephy_struct *fp_next;
};

/* Physical memory device (RAM or SWAP) */
struct memphy_struct {
    BYTE *storage;
    int maxsz;
    /* Sequential device fields */
    int rdmflg;
    int cursor;
    /* Frame management */
    struct framephy_struct *free_fp_list;
    struct framephy_struct *used_fp_list;
};

/* Page number list node — used for FIFO page replacement */
struct pgn_t {
    addr_t pgn;
    struct pgn_t *pg_next;
};

/* Kernel slab cache pool descriptor */
struct kcache_pool_struct {
    int size;    /* aligned slot size */
    int align;   /* alignment unit */
    addr_t storage; /* base address of first allocated slot */
};

/* Memory region descriptor */
struct vm_rg_struct {
    unsigned long rg_start;
    unsigned long rg_end;
    /* Privilege mode bit: user=1, kernel=0 */
    unsigned long mode_bit;
    int vmaid;   /* owning VM area ID */
    struct vm_rg_struct *rg_next;
};

/* Forward declaration needed by vm_area_struct */
struct mm_struct;

/* Virtual memory area descriptor */
struct vm_area_struct {
    unsigned long vm_id;
    unsigned long vm_start;
    unsigned long vm_end;
    unsigned long sbrk;   /* heap break pointer */
    struct mm_struct *vm_mm;
    struct vm_rg_struct *vm_freerg_list;
    struct vm_area_struct *vm_next;
};

/* Per-process memory mapping */
struct mm_struct {
#ifdef MM64
    addr_t *pgd;   /* page global directory (level 5) */
    addr_t *p4d;   /* page level-4 directory */
    addr_t *pud;   /* page upper directory */
    addr_t *pmd;   /* page middle directory */
    addr_t *pt;    /* page table (level 1) */
#else
    uint32_t *pgd; /* flat page table for 32-bit mode */
#endif
    struct vm_area_struct *mmap;
    struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ];
    struct pgn_t *fifo_pgn;
    struct kcache_pool_struct *kcpooltbl;
    struct tlb_entry_t tlb[TLB_SIZE];
    int tlb_replace; /* next TLB victim index (round-robin) */
};

#endif /* OSMM_H */

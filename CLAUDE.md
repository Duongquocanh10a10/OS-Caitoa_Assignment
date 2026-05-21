# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **ossim** (OS Simulator), a C-based educational operating system simulator for course CO2018 at HCMC University of Technology. It simulates process scheduling, virtual memory management with paging, and syscalls on a multi-CPU architecture using pthreads.

## Build & Run

```bash
# Build the OS simulator
make

# Run a specific scenario (input configs are in input/)
./os <input_name>           # e.g., ./os os_1_mlq_paging

# Run all scenarios and capture output
bash run.sh

# Clean build artifacts
make clean
```

The `make` target builds the `os` binary. The `mem` and `sched` targets build standalone module testers (rarely used).

The syscall table (`src/syscalltbl.lst`) is auto-generated from `src/syscall.tbl` via `src/syscalltbl.sh` as part of the `os` build.

## Compile-Time Configuration (`include/os-cfg.h`)

All major features are toggled here:
- `MLQ_SCHED` — enable Multi-Level Queue scheduler (default: on)
- `MM_PAGING` — enable paging-based memory (default: on)
- `MM64` — use 64-bit address mode; comment out for legacy 32-bit (default: on)
- `IODUMP` / `PAGETBL_DUMP` — verbose I/O and page table output
- `VMDBG` / `MMDBG` — VM and memory debug tracing

## Architecture

### Execution Flow

`os.c` reads an input config, spawns one `cpu_routine` pthread per CPU, and a loader thread. Each CPU loop calls `get_proc()` → `run()` → `put_proc()` on a timeslot basis. The scheduler is protected by a mutex.

```
os.c (main + cpu_routine threads)
├── timer.c          — time slot advancement
├── sched.c          — MLQ scheduler (get_proc / put_proc / add_proc)
├── loader.c         — parses input/proc/* files, creates pcb_t
└── cpu.c (run())    — dispatches instructions
    ├── libmem.c     — MM_PAGING alloc/free/read/write (liballoc, libfree, ...)
    │   ├── mm64.c   — 64-bit 5-level page table (active path when MM64 defined)
    │   ├── mm-vm.c  — VMA management, page fault, swap
    │   └── mm-memphy.c — physical RAM + swap device simulation
    └── mem.c        — legacy flat memory (used when MM_PAGING is off)
```

### Scheduler (`src/sched.c`)

MLQ with `MAX_PRIO=140` priority levels. Higher priority → more time slots per round. `slot[i] = MAX_PRIO - i` so priority 0 gets 140 slots. Uses `mlq_ready_queue[MAX_PRIO]` array of queues protected by `pthread_mutex_t queue_lock`.

### Memory Management (paging path)

- **Physical memory**: `memphy_struct` simulates RAM and swap devices as byte arrays with a free-frame list.
- **Virtual memory**: each `pcb_t` has an `mm_struct` with a linked list of `vm_area_struct` (VMAs). Each VMA has a free-region list (`vm_freerg_list`) and a symbol table (`symrgtbl`) mapping region IDs to `vm_rg_struct`.
- **Page tables**: 5-level hierarchy (PGD → P4D → PUD → PMD → PT) implemented in `mm64.c`. Each level stored as dynamically allocated arrays on `krnl_t`.
- **PTE format** (32-bit field): present bit (31), swapped bit (30), dirty (28), FPN (bits 12:0 when present), swap type+offset (bits 25:0 when swapped).
- **`__alloc`** in `libmem.c` finds or extends a VMA, picks free physical frames, sets PTEs, and returns a virtual address.
- **`__free`** reclaims the VMA region back to `vm_freerg_list`.

### Input File Format

**Config file** (e.g., `input/os_1_mlq_paging`):
```
<num_cpus> <time_slot> <num_processes>
<memramsz> <memswpsz_0> <memswpsz_1> ... (hex or decimal)
<start_time> <proc_file> <priority>
...
```

**Process file** (e.g., `input/proc/p0s`):
```
<priority> <num_instructions>
calc
alloc <size> <reg_index>
free <reg_index>
read <src_reg> <offset> <dst_reg>
write <data> <dst_reg> <offset>
kmalloc <size> <reg_index>
kmem_cache_create <pool_id> <size> <reg>
kmem_cache_alloc <pool_id> <reg>
copy_from_user <vmaid> <rgid> <k_vmaid> <k_rgid>
copy_to_user <k_vmaid> <k_rgid> <vmaid> <rgid>
syscall <syscall_id> <arg1> <arg2>
```

`regs[]` in `pcb_t` hold virtual addresses returned by alloc; `reg_index` is used to address them (0–9).

## Key Data Structures

- `pcb_t` (`include/common.h`): process control block — pid, priority, prio (MLQ runtime), code segment, registers, pc, page_table (legacy), krnl pointer
- `krnl_t` (`include/common.h`): kernel context per process — ready/running queues, `mm_struct`, RAM/swap `memphy_struct` pointers, 5-level page directory arrays
- `mm_struct` (`include/os-mm.h`): mmap (VMA list), symrgtbl, pgd pointer
- `memphy_struct`: byte array + free-frame linked list, used for both RAM and swap

## Known Issues

- `src/mm.c` (32-bit path) is marked deprecated and will print `[ERROR]` if called — only the `mm64.c` path is active when `MM64` is defined.
- `src/paging.c` is a standalone test driver (not part of the `os` binary) that hardcodes `input/p0` — it is stale.
- `run.sh` writes outputs to `input/` then moves them to `output/`, but the `os_2_mlq_paging` and `os_2_singleCPU_mlq_paging` configs exist in `input/` without a `run.sh` entry.

# Simple Operating System Simulation

![CI](https://github.com/Linh-N20/ossim_caitoa/actions/workflows/ci.yml/badge.svg)

A simulated Operating System built for **CO2018 — Operating Systems** at Ho Chi Minh City University of Technology (HCMUT), implementing three core OS subsystems: **process scheduling**, **memory management (multi-level paging)**, and **synchronization / system calls**, running over simulated multi-CPU hardware.

> Faculty of Computer Science and Engineering, HCMUT · May 2026

## Overview

The project simulates how an operating system coordinates concurrent processes and manages virtual/physical memory while preserving system integrity. It is built around a dual-mode execution model (user mode vs. privileged kernel mode) and three main subsystems:

- **Scheduler** — a Multi-Level Queue (MLQ) policy across multiple simulated CPUs
- **Memory Management** — a 5-level, 64-bit paging engine with lazy page-table allocation and FIFO swapping
- **Synchronization & System Calls** — mutex-protected shared kernel structures and a unified `syscall()` interface with strict user/kernel space separation

## Features

### CPU Scheduling — Multi-Level Queue (MLQ)
- `MAX_PRIO = 140` priority queues (0 = highest, 139 = lowest)
- Each queue is assigned a fixed slot budget: `slot[p] = MAX_PRIO - p`
- FIFO ordering within a queue; the scheduler scans queues 0 → 139, exhausting each queue's budget before moving on
- Verified against a two-CPU, multi-process execution trace with full Gantt-chart analysis

### Memory Management — 5-Level Paging
- 64-bit virtual address space decomposed into **PGD → P4D → PUD → PMD → PT** (512 entries per level, 4 KB pages)
- Lazy allocation: page-table nodes are only created when an address in their range is first accessed
- Per-process virtual memory represented as a linked list of `vm_area_struct` regions with a heap managed via `sbrk` and a free-region list for reuse
- FIFO page replacement with RAM ↔ SWAP eviction when physical memory is full

### Synchronization
- `queue_lock` protects the MLQ ready queues and running list
- `mmvm_lock` protects allocation/free paths (`__alloc`, `__free`) against concurrent physical-frame assignment
- Physical memory reads/writes are serialized through a single system call, providing implicit ordering

**Notable bug fix:** a shared `krnl->mm` pointer caused one CPU's process to read another process's page table under concurrent execution. Fixed by introducing a per-process `own_mm` field that is restored before every dispatch — see the report for details.

### System Calls
- Unified `syscall(N, ...)` interface dispatched through a `syscall_table`, generated from `syscall.tbl`
- User space never passes a raw PCB pointer — only a PID, which the kernel resolves by walking `running_list`
- Implemented calls: `sys_listsyscall` (index 0) and `sys_memmap` (index 17, covering `MAP`, `INC`, `SWP`, `IO_READ`, and `IO_WRITE` sub-operations)

## Project Structure

```
.
├── include/                    # Header files
│                   
├── src/                        # Kernel source
│   ├── os.c, cpu.c, loader.c, timer.c, queue.c
│   ├── sched.c                                               # MLQ scheduler
│   ├── mm.c, mm-vm.c, mm-memphy.c, mm64.c, paging.c, mem.c   # Memory management
│   ├── libmem.c                                              # alloc/free/read/write (mmvm_lock)
│   ├── sys_mem.c, syscall.c, sys_listsyscall.c               # System call handlers
│   ├── syscall.tbl, syscall.lst, syscalltbl.sh               # Syscall table generation
│   └── libstd.c
├── input/               # Test scenario configs (os_0_mlq_paging, sched_0, ...)
│   └── proc/            # Sample user programs (m0s, s0, sc1, ...)
├── output/              # Expected output for each scenario (<name>.output)
├── Makefile
├── run.sh
└── README.md
```

## Building & Running

```bash
# Build the kernel (produces the `os` binary at the repo root)
make

# Run a specific test scenario
./os os_0_mlq_paging

# Or use the provided helper script
./run.sh
```

To check a run against the expected trace:

```bash
./os os_sc > actual.output
diff output/os_sc.output actual.output
```

## Test Scenarios

| Test | Focus | CI behavior |
|---|---|---|
| `os_sc` | Minimal process lifecycle | **Exact-match** — deterministic, diffed against `output/os_sc.output` |
| `os_syscall_list` | System call table registration | **Exact-match** — deterministic, diffed against expected output |
| `os_0_mlq_paging` | MLQ scheduling + paging, 2 CPUs (Gantt-chart verified) | Informational — output uploaded as artifact |
| `os_1_mlq_paging*` | Page-table operations across concurrent alloc/free/write | Informational |
| `os_1_singleCPU_mlq*` | Single-CPU scheduling and paging stability | Informational |
| `os_2_mlq_paging*` | Multi-CPU paging consistency under heavier load | Informational |
| `os_syscall` | System call + paging interaction | Informational |
| `sched`, `sched_0`, `sched_1` | Pure scheduling behavior (priority & round-robin) | Informational |

> **Why two groups?** Most scenarios run on multiple simulated CPUs, so exact dispatch timing and memory addresses legitimately differ between runs (see the report's per-test comparison notes). Only `os_sc` and `os_syscall_list` are deterministic enough to diff exactly in CI; the rest are run and their output is kept as a build artifact for manual review instead of failing the pipeline on a non-deterministic mismatch.

Full Gantt-chart diagrams, execution traces, and per-test analysis are documented in the project report.

## License

This project was developed for academic purposes as part of the CO2018 Operating Systems course at HCMUT. See `Simple OS Assignment Report.pdf` for the full write-up, Gantt charts, and Q&A discussion. Feel free to reference it for learning purposes.
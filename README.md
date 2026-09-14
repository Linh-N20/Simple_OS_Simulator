# Simple Operating System Simulation

![CI](https://github.com/Linh-N20/Simple_OS_Simulator/actions/workflows/ci.yml/badge.svg)

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
├── include/                # Header files
│                   
├── src/                    # Kernel source
|   |
│   ├── os.c, cpu.c, loader.c, timer.c, queue.c
│   ├── sched.c                                                 # MLQ scheduler
│   ├── mm.c, mm-vm.c, mm-memphy.c, mm64.c, paging.c, mem.c     # Memory management
│   ├── libmem.c                                                # alloc/free/read/write (mmvm_lock)
│   ├── sys_mem.c, syscall.c, sys_listsyscall.c                 # System call handlers
│   ├── syscall.tbl, syscall.lst, syscalltbl.sh                 # Syscall table generation
│   └── libstd.c
|
├── input/                  # Test scenario configs
│   └── proc/               # Sample user programs
|          
├── output/                 # Expected output for each scenario
├── Makefile
├── run.sh
└── README.md
```

## Building & Running

```bash
# Build the kernel
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

| Test | Focus |
|---|---|
| `os_sc` | Minimal process lifecycle |
| `os_syscall_list` | System call table registration |
| `os_0_mlq_paging` | MLQ scheduling + paging, 2 CPUs (Gantt-chart verified) |
| `os_1_mlq_paging*` | Page-table operations across concurrent alloc/free/write |
| `os_1_singleCPU_mlq*` | Single-CPU scheduling and paging stability |
| `os_2_mlq_paging*` | Multi-CPU paging consistency under heavier load |
| `os_syscall` | System call + paging interaction |
| `sched`, `sched_0`, `sched_1` | Pure scheduling behavior (priority & round-robin) |

**On CI behavior:** every scenario relies on real sleep/timer ticks to simulate time slots. On a dedicated local machine, `os_sc` and `os_syscall_list` reproduce `output/*.output` exactly, as documented in the report. On shared GitHub-hosted runners, however, tick timing can drift by a slot under CPU contention, so the CI workflow only fails a scenario on an actual crash (non-zero exit) — it prints an informational diff against `output/*.output` and uploads the real output as an artifact for manual comparison, rather than failing the build on timing drift.

Full Gantt-chart diagrams, execution traces, and per-test analysis are documented in the project report.

## License

This project was developed for academic purposes as part of the CO2018 Operating Systems course at HCMUT. See `Simple OS Assignment Report.pdf` for the full write-up, Gantt charts, and Q&A discussion. Feel free to reference it for learning purposes.
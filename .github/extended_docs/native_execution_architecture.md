# StratOS Native Execution Architecture

## Overview

StratOS can compile and execute native ELF64 binaries. The pipeline:

```
.flux → Flux transpiler → .cpp → Clang → static ELF64 → kernel ELF loader → execute
```

## Components

### boot.S Extensions
- **GDT**: Added User Data (0x18, DPL=3) and User Code (0x20, DPL=3) segments, plus TSS descriptor at 0x28/0x30
- **TSS**: `init_tss` zeros the TSS, sets RSP0 and IST1 to dedicated kernel stacks (16KB each)
- **SYSCALL/SYSRET**: `init_syscall` configures STAR, LSTAR, SFMASK MSRs. STAR = (0x10<<48)|(0x08<<32) for correct segment selection
- **syscall_entry**: Assembly handler saves user context, maps Linux ABI args to C convention, calls `syscall_dispatch()`
- **jump_to_usermode**: IRETQ-based Ring 0 → Ring 3 transition
- **call_elf_ring0**: Direct call for trusted Ring 0 ELF execution

### kernel/core/elf_loader.lx
Full ELF64 parser and loader:
- Validates magic, class (64-bit), endianness (little), machine (x86_64)
- Maps PT_LOAD segments with page-granular allocation
- BSS zeroing for memsz > filesz regions
- 1MB user stack at 0x7FFFFFFFE000 for Ring 3
- 64KB kernel stack for Ring 0

### kernel/core/syscall.lx
~30 Linux-compatible syscalls:
- File I/O: read, write, open, close, stat, fstat, lseek, pread64
- Memory: mmap (anonymous), munmap, brk
- Process: getpid, exit, exit_group
- Filesystem: getcwd, chdir, mkdir, unlink, rename, access, readlink
- System: uname, clock_gettime, getrandom
- TLS: arch_prctl (FS/GS base MSR writes)
- Directory: openat, newfstatat

### kernel/core/process.lx
High-level execution interface:
- `ProcessExec.execute(path, trusted)` — load and run ELF
- `ProcessExec.compileAndRun(source, output, trusted)` — compile + run pipeline
- `ProcessExec.runFlux(fluxPath, trusted)` — full Flux pipeline
- `ProcessExec.updateTssRsp0(kernelStack)` — context switch support

### kernel/memory/manager.lx
Enhanced with user-mode page table support:
- `mapPage()` now propagates the USER bit (0x04) through intermediate page table entries (PML4E, PDPTE, PDE)
- `getOrCreateTable()` accepts a flags parameter and upgrades existing entries if USER bit is needed
- Without this, Ring 3 pages would trigger page faults at any level missing the USER bit

## Toolchain

`tools/build_toolchain.sh` builds a static Clang + LLD + libc++ + musl toolchain:
- 2-stage bootstrap: Stage 1 builds host Clang, Stage 2 builds static musl-linked Clang
- libc++ built with no-exceptions, no-RTTI (matches kernel constraints)
- compiler-rt builtins for __divdi3 etc.
- Wrapper script `stratos-cc` with default flags

## Shell Commands

- `cc <source.cpp> [-o output]` — compile C++ to ELF
- `cc <source.flux> [-o output]` — transpile Flux then compile
- `run <binary> [--trusted]` — execute ELF (Ring 3 default, Ring 0 with --trusted)

## Key Design Decisions

1. **exec is a Flux keyword** — all execution methods use `execute()` not `exec()`
2. **syscall_dispatch needs extern "C"** — added to transpiler's whitelist alongside kernel_main
3. **const char* from c_str()** — use `long` intermediate cast to avoid const-dropping reinterpret_cast
4. **char vs string comparison** — use `'/'` not `"/"` when comparing charAt() results
5. **0xFFFFFFFFFFFFFFFF overflows stoll** — use 0x7FFFFFFFFFFFFFFF for "infinity" sentinel values

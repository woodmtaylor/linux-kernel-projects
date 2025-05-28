# Linux Kernel and Operating Systems Internals

A series of Linux kernel programming projects demonstrating memory management, system calls, multithreading, and block I/O operations on Linux v6.10.

- [Project 2: Basic Kernel Module & Custom System Call](#project-2-basic-kernel-module--custom-system-call)
- [Project 3: Producer-Consumer Kernel Threads](#project-3-producer-consumer-kernel-threads)
- [Project 4: Process Memory Allocation](#project-4-process-memory-allocation)
- [Project 5: USB Block I/O Access](#project-5-usb-block-io-access)

---

## Technical Stack

- Linux kernel v6.10
- C programming
- Make build system
- x86_64 architecture

## Project 2: Basic Kernel Module & Custom System Call

**Objective**: Implement foundational kernel programming concepts including module lifecycle management and custom system call integration.

### Implementation
- **Parameterized Kernel Module**: Created a loadable module that accepts integer and string parameters at load time, demonstrating module parameter handling and kernel logging via `printk()`
- **Custom System Call**: Implemented syscall #463 that logs personalized messages to the kernel ring buffer, requiring kernel recompilation and syscall table modification
- **Userspace Integration**: Developed test program that invokes the custom syscall using the `syscall()` interface

### Files
- `my_name.c` - Kernel module with configurable parameters
- `my_syscall.c` - Custom syscall implementation (syscall #463)
- `syscall_in_userspace_test.c` - Userspace test program
- `Makefile` (2 files) - Build configurations

### Build
```bash
cd project-2-basic-module/
make
sudo insmod my_name.ko [intParameter=2024] [charParameter="Fall"]
gcc -o test syscall_in_userspace_test.c && ./test
dmesg | tail
sudo rmmod my_name
```

---

## Project 3: Producer-Consumer Kernel Threads

**Objective**: Demonstrate kernel-level multithreading and synchronization using the classic producer-consumer problem with semaphores.

### Implementation
- **Kernel Thread Management**: Created configurable numbers of producer and consumer kernel threads using `kthread_run()` with proper naming conventions and lifecycle management
- **Semaphore Synchronization**: Implemented two counting semaphores (`empty` and `full`) to coordinate access between producer and consumer threads, preventing race conditions
- **Graceful Shutdown**: Implemented proper thread termination using `kthread_should_stop()` and `kthread_stop()` to ensure clean module unloading
- **Infinite Loop Design**: Threads run continuously until module removal, simulating real-world kernel thread behavior

### Files
- `producer_consumer.c` - Module implementation
- `Makefile` - Build configuration

### Build
```bash
cd project-3-producer-consumer/
make
sudo insmod producer_consumer.ko [prod=5] [cons=3] [size=10]
dmesg | tail -20
sudo rmmod producer_consumer
```

---

## Project 4: Process Memory Allocation

**Objective**: Build a complete kernel-space memory allocator that handles user process memory requests through virtual device communication and 5-level page table management.

### Implementation
- **Virtual Device Interface**: Created `/dev/memalloc` character device with ioctl handlers for ALLOC and FREE operations, enabling secure user-kernel communication
- **5-Level Page Table Walking**: Implemented complete page table traversal (PGD→P4D→PUD→PMD→PTE) to check existing memory mappings and prevent double allocation
- **Dynamic Page Allocation**: Built page table hierarchy creation system that allocates missing page table levels and maps physical pages with appropriate read/write permissions
- **Resource Management**: Implemented allocation tracking with limits (4096 pages, 100 requests) and proper error handling for resource exhaustion
- **Memory Safety**: Used `copy_from_user()` for secure data transfer and `get_zeroed_page()` for clean page allocation

### Files
```
project-4-memory-allocation/memalloc/
├── memalloc-main.c      # Main module implementation
├── memalloc-helper.c    # Page table helper functions
├── common.h            # Shared structures
├── Makefile           # Build configuration
└── testcases/         # Test suite
```

### Build
```bash
cd project-4-memory-allocation/memalloc/
make
sudo insmod memalloc.ko
ls /dev/memalloc
./test.sh test0
sudo rmmod memalloc
```

---

## Project 5: USB Block I/O Access

**Objective**: Provide direct block-level access to USB storage devices from userspace through kernel module abstraction of the Linux block layer.

### Implementation
- **Block Device Abstraction**: Implemented direct file operations on USB devices using `filp_open()`, `kernel_read()`, and `kernel_write()` to bypass filesystem overhead
- **IOCTL Operation Handlers**: Created four distinct block operations (READ, WRITE, READOFFSET, WRITEOFFSET) supporting both sequential and random access patterns
- **Memory Buffer Management**: Implemented secure buffer handling using `vmalloc()` for kernel space allocation and `copy_from_user()`/`copy_to_user()` for data transfer
- **Offset Tracking**: Built automatic offset management system for sequential operations while supporting explicit offset control for random access
- **Multi-File Architecture**: Designed modular system with separate main module and ioctl handler files for clean code organization

### Files
```
project-5-usb-block-io/kmodule/
├── kmod-main.c      # Main module and USB device management
├── kmod-ioctl.c     # IOCTL handlers for block operations
├── Makefile         # Multi-object build configuration
└── ioctl-defines.h  # Operation definitions (referenced)
```

### Build
```bash
cd project-5-usb-block-io/kmodule/
make
sudo insmod kmod.ko [device=/dev/sdb]
ls /dev/kmod
./test.sh read 512 1 0
sudo rmmod kmod
```

---

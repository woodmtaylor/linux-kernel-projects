# Linux Kernel and Operating Systems Internals

A comprehensive series of Linux kernel programming projects demonstrating memory management, system calls, multithreading, and block I/O operations on Linux v6.10.

## Table of Contents

- [Project 2: Basic Kernel Module & Custom System Call](#project-2-basic-kernel-module--custom-system-call)
- [Project 3: Producer-Consumer Kernel Threads](#project-3-producer-consumer-kernel-threads)
- [Project 4: Process Memory Allocation](#project-4-process-memory-allocation)
- [Project 5: USB Block I/O Access](#project-5-usb-block-io-access)
- [Technical Stack](#technical-stack)
- [Repository Structure](#repository-structure)

---

## Project 2: Basic Kernel Module & Custom System Call

Basic kernel module with parameters and custom system call implementation for Linux v6.10.

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

### Technical Details
- Module parameters: `intParameter` (int, default: 2025), `charParameter` (char*, default: "Spring")
- Custom syscall logs message to kernel ring buffer
- Uses `printk()`, `module_param()`, and syscall registration APIs

---

## Project 3: Producer-Consumer Kernel Threads

Multithreaded producer-consumer implementation using kernel threads and semaphore synchronization.

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

### Technical Details
- Module parameters: `prod` (producers), `cons` (consumers), `size` (empty semaphore init)
- Creates named kernel threads: "Producer-X", "Consumer-X"
- Uses counting semaphores (`empty`, `full`) for synchronization
- APIs: `kthread_run()`, `kthread_stop()`, `sema_init()`, `down_interruptible()`, `up()`

---

## Project 4: Process Memory Allocation

Kernel memory allocator using 5-level page tables and virtual device communication.

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

### Technical Details
- Virtual device: `/dev/memalloc` for ioctl communication
- 5-level page tables: PGD→P4D→PUD→PMD→PTE traversal
- Memory allocation with read/write permissions
- Limits: 4096 pages max, 100 allocation requests max
- APIs: `get_zeroed_page()`, `set_pte_at()`, `copy_from_user()`

---

## Project 5: USB Block I/O Access

Kernel module providing direct block-level access to USB storage devices through ioctl operations and Linux block abstraction layer.

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

### Technical Details
- Block Operations: READ, WRITE, READOFFSET, WRITEOFFSET via ioctl
- USB Device Access: Direct file operations on `/dev/sdb` (configurable)
- Block Abstraction: Uses `kernel_read()` and `kernel_write()` for 512-byte operations
- Memory Management: Secure user-kernel data transfer with `vmalloc()` buffers
- APIs: `filp_open()`, `kernel_read()`, `kernel_write()`, `copy_from_user()`, `copy_to_user()`

---

## Technical Stack

- Linux kernel v6.10
- C programming
- Make build system
- x86_64 architecture

## Repository Structure

```
linux-kernel-projects/
├── README.md
├── project-2-basic-module/
├── project-3-producer-consumer/
├── project-4-memory-allocation/
└── project-5-usb-block-io/
```
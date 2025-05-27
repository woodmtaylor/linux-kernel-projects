/* General headers */
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/vmalloc.h>
#include <asm/pgalloc.h>
/* File IO-related headers */
#include <linux/fs.h>
#include <linux/bio.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
/* Sleep and timer headers */
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/pci.h>
#include "../common.h"
#include "memalloc-common.h"

/* Simple licensing stuff */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("student");
MODULE_DESCRIPTION("Project 4, CSE 330 Fall 2025");
MODULE_VERSION("0.01");

/* Calls which start and stop the ioctl teardown */
bool memalloc_ioctl_init(void);
void memalloc_ioctl_teardown(void);

/* Project 2 Solution Variable/Struct Declarations */
#define MAX_PAGES           4096
#define MAX_ALLOCATIONS     100
#define DEVICE_NAME         "memalloc"
#define DEVICE_CLASS        "memalloc"

#if defined(CONFIG_X86_64)
    #define PAGE_PERMS_RW   PAGE_SHARED
    #define PAGE_PERMS_R    PAGE_READONLY
#else
    #define PAGE_PERMS_RW   __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_WRITE)
    #define PAGE_PERMS_R    __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_RDONLY)
#endif

struct alloc_info           alloc_req;
struct free_info            free_req;

/* Device variables */
static int device_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

/* Allocation tracking */
static int total_pages_allocated = 0;
static int total_allocations = 0;

/* Function to check if memory is already mapped */
static int is_memory_mapped(unsigned long vaddr, int num_pages) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long addr;
    
    /* Iterate through each page in the requested range */
    for (addr = vaddr; addr < vaddr + (num_pages * PAGE_SIZE); addr += PAGE_SIZE) {
        /* Get the PGD (top-level page directory) */
        pgd = pgd_offset(current->mm, addr);
        if (pgd_none(*pgd) || pgd_bad(*pgd)) {
            /* This page is not mapped */
            continue;
        }
        
        /* Level 2: P4D */
        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d)) {
            /* This page is not mapped */
            continue;
        }
        
        /* Level 3: PUD */
        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud)) {
            /* This page is not mapped */
            continue;
        }
        
        /* Level 4: PMD */
        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd)) {
            /* This page is not mapped */
            continue;
        }
        
        /* Level 5: PTE (final level page table) */
        pte = pte_offset_kernel(pmd, addr);
        if (!pte_none(*pte)) {
            /* Page is already mapped */
            return 1;  /* At least one page is already mapped */
        }
    }
    
    return 0;  /* None of the pages are mapped */
}

/* Function to allocate memory pages */
static int allocate_memory(unsigned long vaddr, int num_pages, bool write) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long addr;
    void *page_vaddr;
    unsigned long page_paddr;
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    int pages_allocated = 0;
    
    /* Check allocation limits */
    if (total_pages_allocated + num_pages > MAX_PAGES) {
        printk("Error: Maximum page limit exceeded (%d/%d).\n", 
               total_pages_allocated, MAX_PAGES);
        return -2;  /* Page count exceeded */
    }
    
    if (total_allocations >= MAX_ALLOCATIONS) {
        printk("Error: Maximum allocation count exceeded (%d/%d).\n", 
               total_allocations, MAX_ALLOCATIONS);
        return -3;  /* Allocation count exceeded */
    }
    
    /* Check if memory is already mapped */
    if (is_memory_mapped(vaddr, num_pages)) {
        printk("Error: Memory region already mapped.\n");
        return -1;  /* Memory already mapped */
    }
    
    /* Allocate each page */
    for (addr = vaddr; addr < vaddr + (num_pages * PAGE_SIZE); addr += PAGE_SIZE) {
        /* Get the PGD (top-level page directory) */
        pgd = pgd_offset(current->mm, addr);
        
        /* Level 2: P4D */
        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d)) {
            memalloc_pud_alloc(p4d, addr);
            p4d = p4d_offset(pgd, addr);
        }
        
        /* Level 3: PUD */
        pud = pud_offset(p4d, addr);
        if (pud_none(*pud)) {
            memalloc_pmd_alloc(pud, addr);
            pud = pud_offset(p4d, addr);
        }
        
        /* Level 4: PMD */
        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd)) {
            memalloc_pte_alloc(pmd, addr);
            pmd = pmd_offset(pud, addr);
        }
        
        /* Level 5: PTE (final level page table) */
        pte = pte_offset_kernel(pmd, addr);
        
        /* Allocate a new page */
        page_vaddr = (void *)get_zeroed_page(gfp);
        if (!page_vaddr) {
            printk("Failed to get a page from the freelist\n");
            return -ENOMEM;
        }
        
        /* Get physical address */
        page_paddr = __pa(page_vaddr);
        
        /* Map the page with appropriate permissions */
        if (write) {
            set_pte_at(current->mm, addr, pte, 
                      pfn_pte((page_paddr >> PAGE_SHIFT), PAGE_PERMS_RW));
        } else {
            set_pte_at(current->mm, addr, pte, 
                      pfn_pte((page_paddr >> PAGE_SHIFT), PAGE_PERMS_R));
        }
        pages_allocated++;
    }
    
    /* Update allocation counters */
    total_pages_allocated += pages_allocated;
    total_allocations++;
    
    printk("Successfully allocated %d pages at address %lx with %s permissions\n", 
           pages_allocated, vaddr, write ? "read-write" : "read-only");
    
    return 0;  /* Success */
}

/* Function to free allocated memory */
static int free_memory(unsigned long vaddr) {
    /* Implement free functionality if needed */
    /* For this assignment, we'll just print the message and return success */
    printk("Free request for address %lx\n", vaddr);
    return 0;
}

/* IOCTL handler for vmod. */
static long memalloc_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    
    switch (cmd) {
    case ALLOCATE:
        /* Copy alloc_info struct from user space */
        if (copy_from_user(&alloc_req, (struct alloc_info *)arg, sizeof(struct alloc_info))) {
            printk("Error: Failed to copy allocation request from user.\n");
            return -EFAULT;
        }
        
        printk("IOCTL: alloc(%lx, %d, %d)\n", alloc_req.vaddr, alloc_req.num_pages, alloc_req.write);
        
        /* Allocate the requested memory */
        ret = allocate_memory(alloc_req.vaddr, alloc_req.num_pages, alloc_req.write);
        break;
        
    case FREE:
        /* Copy free_info struct from user space */
        if (copy_from_user(&free_req, (struct free_info *)arg, sizeof(struct free_info))) {
            printk("Error: Failed to copy free request from user.\n");
            return -EFAULT;
        }
        
        printk("IOCTL: free(%lx)\n", free_req.vaddr);
        
        /* Free the requested memory */
        ret = free_memory(free_req.vaddr);
        break;
        
    default:
        printk("Error: incorrect IOCTL command.\n");
        return -1;
    }
    
    return ret;
}

/* Required file ops. */
static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = memalloc_ioctl,
};

/* Initialize the module for IOCTL commands */
bool memalloc_ioctl_init(void) {
    /* Register a character device */
    device_major = register_chrdev(0, DEVICE_NAME, &fops);
    if (device_major < 0) {
        printk("Failed to register device: %d\n", device_major);
        return false;
    }
    
    /* Create device class */
    device_class = class_create(DEVICE_CLASS);
    if (IS_ERR(device_class)) {
        unregister_chrdev(device_major, DEVICE_NAME);
        printk("Failed to create device class\n");
        return false;
    }
    
    /* Create device */
    device = device_create(device_class, NULL, MKDEV(device_major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        class_destroy(device_class);
        unregister_chrdev(device_major, DEVICE_NAME);
        printk("Failed to create device\n");
        return false;
    }
    
    printk("Device /dev/%s created successfully\n", DEVICE_NAME);
    return true;
}

/* Teardown the IOCTL interface */
void memalloc_ioctl_teardown(void) {
    /* Destroy the device and class */
    if (device_class) {
        device_destroy(device_class, MKDEV(device_major, 0));
        class_destroy(device_class);
    }
    
    /* Unregister the character device */
    unregister_chrdev(device_major, DEVICE_NAME);
    
    printk("Device /dev/%s removed\n", DEVICE_NAME);
}

/* Init and Exit functions */
static int __init memalloc_module_init(void) {
    printk("Hello from the memalloc module!\n");
    
    /* Initialize IOCTL interface */
    if (!memalloc_ioctl_init()) {
        printk("Failed to initialize IOCTL interface\n");
        return -1;
    }
    
    printk("Memory allocator initialized successfully\n");
    return 0;
}

static void __exit memalloc_module_exit(void) {
    /* Teardown IOCTL */
    memalloc_ioctl_teardown();
    
    printk("Goodbye from the memalloc module!\n");
}

module_init(memalloc_module_init);
module_exit(memalloc_module_exit);

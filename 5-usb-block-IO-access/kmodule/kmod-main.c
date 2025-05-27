#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>

/* File IO-related headers */
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#include <linux/blkpg.h>     
#include <linux/namei.h>     

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Adil Ahmad");
MODULE_DESCRIPTION("A Block Abstraction Read/Write for a USB device.");
MODULE_VERSION("1.0");

/* USB device name argument */
char* device = "/dev/sdb";
module_param(device, charp, S_IRUGO);

/* USB storage disk-related data structures - important: not static */
struct file *usb_file = NULL;
EXPORT_SYMBOL(usb_file); // Export this symbol for use in other files

bool kmod_ioctl_init(void);
void kmod_ioctl_teardown(void);

static bool open_usb(void)
{
    /* Open a file for the path of the usb */
    printk(KERN_INFO "Opening USB device: %s\n", device);
    
    usb_file = filp_open(device, O_RDWR, 0);
    if (IS_ERR(usb_file)) {
        printk(KERN_ERR "Failed to open device file %s, error: %ld\n", 
               device, PTR_ERR(usb_file));
        return false;
    }
    
    printk(KERN_INFO "Successfully opened USB device: %s\n", device);
    return true;
}

static void close_usb(void)
{
    /* Close the file and device communication interface */
    if (usb_file && !IS_ERR(usb_file)) {
        filp_close(usb_file, NULL);
        usb_file = NULL;
        printk(KERN_INFO "Closed device file\n");
    }
    
    printk(KERN_INFO "USB device closed successfully\n");
}

static int __init kmod_init(void)
{
    pr_info("Block IO Kernel Module loading\n");
    if (!open_usb()) {
        pr_err("Failed to open USB block device\n");
        return -ENODEV;
    }
    
    if (!kmod_ioctl_init()) {
        close_usb();
        pr_err("Failed to initialize IOCTL interface\n");
        return -EFAULT;
    }
    
    pr_info("Block IO Kernel Module loaded successfully\n");
    return 0;
}

static void __exit kmod_fini(void)
{
    close_usb();
    kmod_ioctl_teardown();
    printk("Block IO Kernel Module unloaded\n");
}

module_init(kmod_init);
module_exit(kmod_fini);

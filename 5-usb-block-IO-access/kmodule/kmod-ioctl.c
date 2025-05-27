#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <linux/ioctl.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>

#include <linux/cdev.h>
#include <linux/nospec.h>

#include "../ioctl-defines.h"

#include <linux/vmalloc.h>

/* Device-related definitions */
static dev_t            dev = 0;
static struct class*    kmod_class;
static struct cdev      kmod_cdev;

/* Current offset in the device */
static unsigned long current_offset = 0;

/* Buffers for different operation requests */
struct block_rw_ops rw_request;
struct block_rwoffset_ops rwoffset_request;

/* External declarations */
extern struct file *usb_file;

bool kmod_ioctl_init(void);
void kmod_ioctl_teardown(void);

static long kmod_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    char* kernbuf;
    unsigned int size, offset;
    ssize_t bytes;
    loff_t pos;
    
    printk(KERN_INFO "IOCTL command received: %u\n", cmd);
    
    switch (cmd)
    {
        case BREAD:
            // Copy the request struct from user space
            if (copy_from_user(&rw_request, (void *)arg, sizeof(struct block_rw_ops))) {
                printk(KERN_ERR "Failed to copy request from user\n");
                return -EFAULT;
            }
            
            size = rw_request.size;
            pos = current_offset;
            printk(KERN_INFO "READ: size=%u, offset=%lld\n", size, pos);
            
            // Allocate kernel buffer for the operation
            kernbuf = vmalloc(size);
            if (!kernbuf) {
                printk(KERN_ERR "Failed to allocate kernel buffer\n");
                return -ENOMEM;
            }
            
            // Read from file
            bytes = kernel_read(usb_file, kernbuf, size, &pos);
            if (bytes < 0) {
                printk(KERN_ERR "Failed to read from file, error: %zd\n", bytes);
                vfree(kernbuf);
                return bytes;
            }
            
            // Copy data back to user space
            if (copy_to_user(rw_request.data, kernbuf, bytes)) {
                printk(KERN_ERR "Failed to copy data to user\n");
                vfree(kernbuf);
                return -EFAULT;
            }
            
            vfree(kernbuf);
            current_offset = pos;  // Update the current offset
            printk(KERN_INFO "READ completed: read %zd bytes, new offset=%lu\n", bytes, current_offset);
            return bytes;
            
        case BWRITE:
            // Copy the request struct from user space
            if (copy_from_user(&rw_request, (void *)arg, sizeof(struct block_rw_ops))) {
                printk(KERN_ERR "Failed to copy request from user\n");
                return -EFAULT;
            }
            
            size = rw_request.size;
            pos = current_offset;
            printk(KERN_INFO "WRITE: size=%u, offset=%lld\n", size, pos);
            
            // Allocate kernel buffer for the operation
            kernbuf = vmalloc(size);
            if (!kernbuf) {
                printk(KERN_ERR "Failed to allocate kernel buffer\n");
                return -ENOMEM;
            }
            
            // Copy data from user space
            if (copy_from_user(kernbuf, rw_request.data, size)) {
                printk(KERN_ERR "Failed to copy data from user\n");
                vfree(kernbuf);
                return -EFAULT;
            }
            
            // Write to file
            bytes = kernel_write(usb_file, kernbuf, size, &pos);
            if (bytes < 0) {
                printk(KERN_ERR "Failed to write to file, error: %zd\n", bytes);
                vfree(kernbuf);
                return bytes;
            }
            
            vfree(kernbuf);
            current_offset = pos;  // Update the current offset
            printk(KERN_INFO "WRITE completed: wrote %zd bytes, new offset=%lu\n", bytes, current_offset);
            return bytes;
            
        case BREADOFFSET:
            // Copy the request struct from user space
            if (copy_from_user(&rwoffset_request, (void *)arg, sizeof(struct block_rwoffset_ops))) {
                printk(KERN_ERR "Failed to copy request from user\n");
                return -EFAULT;
            }
            
            size = rwoffset_request.size;
            offset = rwoffset_request.offset;
            pos = offset;
            printk(KERN_INFO "READOFFSET: size=%u, offset=%u\n", size, offset);
            
            // Allocate kernel buffer for the operation
            kernbuf = vmalloc(size);
            if (!kernbuf) {
                printk(KERN_ERR "Failed to allocate kernel buffer\n");
                return -ENOMEM;
            }
            
            // Read from file
            bytes = kernel_read(usb_file, kernbuf, size, &pos);
            if (bytes < 0) {
                printk(KERN_ERR "Failed to read from file, error: %zd\n", bytes);
                vfree(kernbuf);
                return bytes;
            }
            
            // Copy data back to user space
            if (copy_to_user(rwoffset_request.data, kernbuf, bytes)) {
                printk(KERN_ERR "Failed to copy data to user\n");
                vfree(kernbuf);
                return -EFAULT;
            }
            
            vfree(kernbuf);
            current_offset = pos;  // Update the current offset
            printk(KERN_INFO "READOFFSET completed: read %zd bytes, new offset=%lu\n", bytes, current_offset);
            return bytes;
            
        case BWRITEOFFSET:
            // Copy the request struct from user space
            if (copy_from_user(&rwoffset_request, (void *)arg, sizeof(struct block_rwoffset_ops))) {
                printk(KERN_ERR "Failed to copy request from user\n");
                return -EFAULT;
            }
            
            size = rwoffset_request.size;
            offset = rwoffset_request.offset;
            pos = offset;
            printk(KERN_INFO "WRITEOFFSET: size=%u, offset=%u\n", size, offset);
            
            // Allocate kernel buffer for the operation
            kernbuf = vmalloc(size);
            if (!kernbuf) {
                printk(KERN_ERR "Failed to allocate kernel buffer\n");
                return -ENOMEM;
            }
            
            // Copy data from user space
            if (copy_from_user(kernbuf, rwoffset_request.data, size)) {
                printk(KERN_ERR "Failed to copy data from user\n");
                vfree(kernbuf);
                return -EFAULT;
            }
            
            // Write to file
            bytes = kernel_write(usb_file, kernbuf, size, &pos);
            if (bytes < 0) {
                printk(KERN_ERR "Failed to write to file, error: %zd\n", bytes);
                vfree(kernbuf);
                return bytes;
            }
            
            vfree(kernbuf);
            current_offset = pos;  // Update the current offset
            printk(KERN_INFO "WRITEOFFSET completed: wrote %zd bytes, new offset=%lu\n", bytes, current_offset);
            return bytes;
            
        default: 
            printk(KERN_ERR "Error: incorrect operation requested, returning.\n");
            return -EINVAL;
    }
    return 0;
}

static int kmod_open(struct inode* inode, struct file* file) {
    printk("Opened kmod. \n");
    return 0;
}

static int kmod_release(struct inode* inode, struct file* file) {
    printk("Closed kmod. \n");
    return 0;
}

static struct file_operations fops = 
{
    .owner          = THIS_MODULE,
    .open           = kmod_open,
    .release        = kmod_release,
    .unlocked_ioctl = kmod_ioctl,
};

/* Initialize the module for IOCTL commands */
bool kmod_ioctl_init(void) {
    /* Allocate a character device. */
    if (alloc_chrdev_region(&dev, 0, 1, "usbaccess") < 0) {
        printk("error: couldn't allocate \'usbaccess\' character device.\n");
        return false;
    }
    
    /* Initialize the chardev with my fops. */
    cdev_init(&kmod_cdev, &fops);
    if (cdev_add(&kmod_cdev, dev, 1) < 0) {
        printk("error: couldn't add kmod_cdev.\n");
        goto cdevfailed;
    }
    
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6,2,16)
    if ((kmod_class = class_create(THIS_MODULE, "kmod_class")) == NULL) {
        printk("error: couldn't create kmod_class.\n");
        goto cdevfailed;
    }
#else
    if ((kmod_class = class_create("kmod_class")) == NULL) {
        printk("error: couldn't create kmod_class.\n");
        goto cdevfailed;
    }
#endif
    
    if ((device_create(kmod_class, NULL, dev, NULL, "kmod")) == NULL) {
        printk("error: couldn't create device.\n");
        goto classfailed;
    }
    
    printk("[*] IOCTL device initialization complete.\n");
    return true;
    
classfailed:
    class_destroy(kmod_class);
cdevfailed:
    unregister_chrdev_region(dev, 1);
    return false;
}

void kmod_ioctl_teardown(void) {
    /* Destroy the classes too (IOCTL-specific). */
    if (kmod_class) {
        device_destroy(kmod_class, dev);
        class_destroy(kmod_class);
    }
    
    cdev_del(&kmod_cdev);
    unregister_chrdev_region(dev,1);
    printk("[*] IOCTL device teardown complete.\n");
}

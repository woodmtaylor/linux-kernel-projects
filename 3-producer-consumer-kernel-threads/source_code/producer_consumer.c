#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Taylor Wood");
MODULE_DESCRIPTION("Producer Consumer Module with Semaphores");

/* Module parameters */
static int prod = 2;  // Number of producer threads
module_param(prod, int, 0644);
MODULE_PARM_DESC(prod, "Number of producer threads");

static int cons = 2;  // Number of consumer threads
module_param(cons, int, 0644);
MODULE_PARM_DESC(cons, "Number of consumer threads");

static int size = 5;  // Size for the empty semaphore
module_param(size, int, 0644);
MODULE_PARM_DESC(size, "Initial value for the empty semaphore");

/* Global variables */
static struct task_struct **producer_threads;  // Array to hold producer thread pointers
static struct task_struct **consumer_threads;  // Array to hold consumer thread pointers

/* Semaphores for synchronization */
static struct semaphore empty;  // Counts empty buffer slots
static struct semaphore full;   // Counts filled buffer slots

/* Producer thread function */
static int producer_function(void *arg)
{
    int id = *(int *)arg;
    char thread_name[TASK_COMM_LEN];
    
    get_task_comm(thread_name, current);
    printk(KERN_INFO "Producer thread started: %s\n", thread_name);

    /* Producer runs until module is unloaded */
    while (!kthread_should_stop()) {
        /* Wait for an empty slot */
        if (down_interruptible(&empty)) {
            printk(KERN_INFO "Producer-%d: Interrupted while waiting for empty slot\n", id);
            break;
        }
        
        /* Produce an item */
        printk(KERN_INFO "An item has been produced by Producer-%d\n", id);
        
        /* Signal that a slot is filled */
        up(&full);
        
        /* Add a small delay to make the output readable */
        msleep(1000);
    }
    
    printk(KERN_INFO "Producer-%d exiting\n", id);
    return 0;
}

/* Consumer thread function */
static int consumer_function(void *arg)
{
    int id = *(int *)arg;
    char thread_name[TASK_COMM_LEN];
    
    get_task_comm(thread_name, current);
    printk(KERN_INFO "Consumer thread started: %s\n", thread_name);

    /* Consumer runs until module is unloaded */
    while (!kthread_should_stop()) {
        /* Wait for a filled slot */
        if (down_interruptible(&full)) {
            printk(KERN_INFO "Consumer-%d: Interrupted while waiting for filled slot\n", id);
            break;
        }
        
        /* Consume an item */
        printk(KERN_INFO "An item has been consumed by Consumer-%d\n", id);
        
        /* Signal that a slot is empty */
        up(&empty);
        
        /* Add a small delay to make the output readable */
        msleep(1000);
    }
    
    printk(KERN_INFO "Consumer-%d exiting\n", id);
    return 0;
}

/* Module initialization function */
static int __init producer_consumer_init(void)
{
    int i;
    int *id;
    
    printk(KERN_INFO "Producer Consumer module loading...\n");
    
    /* Validate parameters */
    if (prod < 0 || cons < 0 || size < 0) {
        printk(KERN_ERR "Invalid parameters: prod=%d, cons=%d, size=%d\n", prod, cons, size);
        return -EINVAL;
    }
    
    /* Allocate memory for thread arrays */
    producer_threads = kmalloc(prod * sizeof(struct task_struct *), GFP_KERNEL);
    if (!producer_threads && prod > 0) {
        printk(KERN_ERR "Failed to allocate memory for producer threads\n");
        return -ENOMEM;
    }
    
    consumer_threads = kmalloc(cons * sizeof(struct task_struct *), GFP_KERNEL);
    if (!consumer_threads && cons > 0) {
        printk(KERN_ERR "Failed to allocate memory for consumer threads\n");
        kfree(producer_threads);
        return -ENOMEM;
    }
    
    /* Initialize semaphores */
    sema_init(&empty, size);  // Initially, 'size' empty slots
    sema_init(&full, 0);      // Initially, no slots are filled
    
    /* Create producer threads */
    for (i = 0; i < prod; i++) {
        id = kmalloc(sizeof(int), GFP_KERNEL);
        if (!id) {
            printk(KERN_ERR "Failed to allocate memory for producer ID\n");
            goto cleanup_producer_threads;
        }
        
        *id = i + 1;  // Thread IDs start from 1
        producer_threads[i] = kthread_run(producer_function, id, "Producer-%d", *id);
        
        if (IS_ERR(producer_threads[i])) {
            printk(KERN_ERR "Failed to create producer thread %d\n", i + 1);
            kfree(id);
            goto cleanup_producer_threads;
        }
    }
    
    /* Create consumer threads */
    for (i = 0; i < cons; i++) {
        id = kmalloc(sizeof(int), GFP_KERNEL);
        if (!id) {
            printk(KERN_ERR "Failed to allocate memory for consumer ID\n");
            goto cleanup_consumer_threads;
        }
        
        *id = i + 1;  // Thread IDs start from 1
        consumer_threads[i] = kthread_run(consumer_function, id, "Consumer-%d", *id);
        
        if (IS_ERR(consumer_threads[i])) {
            printk(KERN_ERR "Failed to create consumer thread %d\n", i + 1);
            kfree(id);
            goto cleanup_consumer_threads;
        }
    }
    
    printk(KERN_INFO "Producer Consumer module loaded successfully with %d producers and %d consumers\n", prod, cons);
    return 0;

cleanup_consumer_threads:
    for (i = 0; i < cons; i++) {
        if (consumer_threads[i] && !IS_ERR(consumer_threads[i])) {
            kthread_stop(consumer_threads[i]);
        }
    }

cleanup_producer_threads:
    for (i = 0; i < prod; i++) {
        if (producer_threads[i] && !IS_ERR(producer_threads[i])) {
            kthread_stop(producer_threads[i]);
        }
    }
    
    kfree(producer_threads);
    kfree(consumer_threads);
    
    return -EFAULT;
}

/* Module exit function */
static void __exit producer_consumer_exit(void)
{
    int i;
    
    printk(KERN_INFO "Producer Consumer module unloading...\n");
    
    /* Stop and clean up consumer threads */
    for (i = 0; i < cons; i++) {
        if (consumer_threads[i] && !IS_ERR(consumer_threads[i])) {
            kthread_stop(consumer_threads[i]);
            printk(KERN_INFO "Stopped Consumer-%d\n", i + 1);
        }
    }
    
    /* Stop and clean up producer threads */
    for (i = 0; i < prod; i++) {
        if (producer_threads[i] && !IS_ERR(producer_threads[i])) {
            kthread_stop(producer_threads[i]);
            printk(KERN_INFO "Stopped Producer-%d\n", i + 1);
        }
    }
    
    /* Free allocated memory */
    kfree(producer_threads);
    kfree(consumer_threads);
    
    printk(KERN_INFO "Producer Consumer module unloaded successfully\n");
}

module_init(producer_consumer_init);
module_exit(producer_consumer_exit);

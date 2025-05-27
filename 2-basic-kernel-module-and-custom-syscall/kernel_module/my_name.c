#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Taylor Wood");

// Module parameters (declaration and registration)
static int intParameter = 2025;
module_param(intParameter, int, 0);

static char *charParameter = "Spring";
module_param(charParameter, charp, 0);

// Initialization function
static int __init my_module_init(void) {
	printk(KERN_INFO "Hello, I am %s, a student of CSE330 %s %d. \n",
			"Taylor Wood", charParameter, intParameter);
	return 0;
}

// Exit function
static void __exit my_module_exit(void) {
	// Currently empty
}


module_init(my_module_init);
module_exit(my_module_exit);

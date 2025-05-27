#include <linux/kernel.h>
#include <linux/syscalls.h>

asmlinkage long __x64_sys_my_syscall(void) {
	printk(KERN_INFO "This is the new system call Taylor Wood implemented.\n");
	return 0;
}

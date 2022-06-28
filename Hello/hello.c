#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("quardell");

static int __init  start_module(void) {
    printk(KERN_INFO "Hello world\n");
    return 0;
}

static void __exit end_module(void) {
    printk(KERN_INFO "Goodbye world\n");
}

module_init(start_module);
module_exit(end_module);
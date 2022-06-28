#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/fs.h>

#include "basicftfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("quardell");

struct Dentry *basicftfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    struct dentry *dentry = mount_bdev(fs_type, flags, dev_name, data, basicftfs_fill_super);

    if (IS_ERR(dentry)) {
        printk(KERN_ERR "Mount failed\n");
    } else {
        printk(KERN_INFO "Mount succesfull\n");
    }

    return dentry;
}

void basicftfs_kill_sb(struct superblock *sb) {
    printk(KERN_INFO "Superblock will be destryoyed\n");
    kill_block_super(sb);
}

static struct file_system_type basicftfs_type = {
    .owner = THIS_MODULE,
    .name = "basicftfs",
    .mount = basicftfs_mount,
    .kill_sb = basicftfs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init  basicftfs_init(void) {
    int ret = basicftfs_init_inode_cache();

    ret = register_filesystem(&basicftfs_type);

    if (ret) {
        printk(KERN_ERR "Failed registration of filesystem\n");
    }
    printk(KERN_INFO "Fileystem registered succesfully\n");
    return 0;
}

static void __exit basicftfs_exit(void) {
    int ret = unregister_filesystem(&basicftfs_type);
    if (ret) {
        printk(KERN_ERR "Failed unregistration of filesystem\n");
    }

    simplefatfs_destroy_inode_cache();

    printk(KERN_INFO "remove module\n");
}

module_init(basicftfs_init);
module_exit(basicftfs_exit);

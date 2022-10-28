#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "basicbtfs.h"
#include "cache.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("quardell");

/* Mount a basicftfs partition */
struct dentry *basicbtfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    struct dentry *dentry = mount_bdev(fs_type, flags, dev_name, data, basicbtfs_fill_super);

    if (IS_ERR(dentry)) {
        printk(KERN_ERR "Mount failed\n");
    } else {
        printk(KERN_INFO "Mount succesfull\n");
    }

    return dentry;
}

/* Unmount a basicftfs partition */
void basicbtfs_kill_sb(struct super_block *sb) {
    printk(KERN_INFO "disk will be destroyed\n");
    kill_block_super(sb);
}

static struct file_system_type basicftfs_file_system_type = {
    .owner = THIS_MODULE,
    .name = "basicbtfs",
    .mount = basicbtfs_mount,
    .kill_sb = basicbtfs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init basicbtfs_init(void) {
    int ret = basicbtfs_init_inode_cache();
    if (ret) {
        printk(KERN_ERR "Inode cache creation failed\n");
        return ret;
    }

    ret = basicbtfs_init_btree_dir_cache();

    if (ret) {
        printk(KERN_ERR "btree directory cache creation failed\n");
        return ret;
    }

    ret = basicbtfs_init_btree_node_data_cache();

    if (ret) {
        printk(KERN_ERR "btree node data cache creation failed\n");
        return ret;
    }

    ret = basicbtfs_init_nametree_hdr_cache();

    if (ret) {
        printk(KERN_ERR "btree nametree header cache creation failed\n");
        return ret;
    }

    ret = basicbtfs_init_file_cache();

    if (ret) {
        printk(KERN_ERR "btree file block cache creation failed\n");
        return ret;
    }

    ret = register_filesystem(&basicftfs_file_system_type);
    if (ret) {
        printk(KERN_ERR "Failed registration of filesystem\n");
        return ret;
    }

    printk(KERN_INFO "Module registered succesfully\n");
    return ret;
}

static void __exit basicbtfs_exit(void) {
    int ret = unregister_filesystem(&basicftfs_file_system_type);
    if (ret) {
        printk(KERN_ERR "Failed unregistration of filesystem\n");
    }

    basicbtfs_destroy_inode_cache();
    basicbtfs_destroy_btree_node_data_cache();
    basicbtfs_destroy_btree_dir_cache();
    basicbtfs_destroy_nametree_hdr_cache();
    basicbtfs_destroy_file_cache();
    printk(KERN_INFO "Module unregistered succesfully\n");
}

module_init(basicbtfs_init);
module_exit(basicbtfs_exit);

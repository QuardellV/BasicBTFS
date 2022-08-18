
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "bitmap.h"
#include "basicbtfs.h"
#include "defrag.h"

long basicbtfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct inode *inode = file_inode(file);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    // struct basicbtfs_sb_info *sbi = (struct basicbtfs_sb_info *) BASICBTFS_SB(sb);
    printk("something has been sent: %d\n", inode_info->i_bno);

    switch (cmd) {
        case BASICBTFS_IOC_DEFRAG:
            printk("we did it\n");
            return -ENOTTY;
        default:
            return -ENOTTY;
    }
}
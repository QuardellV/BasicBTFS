
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>

#include "bitmap.h"
#include "basicbtfs.h"
#include "defrag.h"

long basicbtfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);

    switch (cmd) {
        case BASICBTFS_IOC_DEFRAG:
            printk("we did it\n");
            break;
        default:
            break;
    }
}
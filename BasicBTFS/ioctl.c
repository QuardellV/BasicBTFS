
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
    // struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct dentry *dentry = sb->s_root;
    int ret = 0;
    // uint32_t first_block = 1 + sbi->s_imap_blocks + sbi->s_bmap_blocks + sbi->s_inode_blocks + sbi->s_filemap_blocks; // root node might be updated :)
    bool is_root = (inode->i_ino == 0) && (dentry->d_name.name && dentry->d_name.name[0] == '/');
    // struct basicbtfs_sb_info *sbi = (struct basicbtfs_sb_info *) BASICBTFS_SB(sb);
    printk("something has been sent: %d and filename %s\n", inode_info->i_bno, dentry->d_name.name);

    switch (cmd) {
        case BASICBTFS_IOC_DEFRAG:
            if (is_root) {
                ret = basicbtfs_defrag_disk(sb, inode);
                if (ret < 0) {
                    printk("something went wrong\n");
                }
            }
            printk("we did it\n");
            return -ENOTTY;
        default:
            return -ENOTTY;
    }
}
#ifndef BASICFTFS_INIT_H
#define BASICFTFS_INIT_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "basicftfs.h"
#include "io.h"

static inline void init_mode_attributes(mode_t mode, struct inode *vfs_inode, struct basicftfs_inode_info *inode_info, uint32_t bno) {
    vfs_inode->i_blocks = 1;
    if (S_ISDIR(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = BASICFTFS_BLOCKSIZE;
        vfs_inode->i_fop = &basicftfs_dir_ops;
        set_nlink(vfs_inode, 2); /* . and .. */
    } else if (S_ISREG(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = 0;
        vfs_inode->i_fop = &basicftfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicftfs_aops;
        set_nlink(vfs_inode, 1);
    }
}

static inline void init_inode_mode(struct inode *vfs_inode, struct basicftfs_inode *disk_inode, struct basicftfs_inode_info *inode_info) {
    if (S_ISDIR(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicftfs_dir_ops;
    } else if (S_ISREG(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicftfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicftfs_aops;
    } else if (S_ISLNK(vfs_inode->i_mode)) {
        strncpy(inode_info->i_data, disk_inode->i_data, sizeof(inode_info->i_data));
        vfs_inode->i_link = inode_info->i_data;
        vfs_inode->i_op = &symlink_inode_ops;
    }
}

#endif

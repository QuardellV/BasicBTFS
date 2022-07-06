#ifndef BASICFTFS_INIT_H
#define BASICFTFS_INIT_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

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

// static inline int init_vfs_inode(struct super_block *sb, struct inode *inode, unsigned long ino) {
//     struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
//     struct basicftfs_inode *bftfs_inode = NULL;
//     struct basicftfs_inode_info *bftfs_inode_info = BASICFTFS_INODE(inode);
//     struct buffer_head *bh = NULL;
//     uint32_t inode_block = BASICFTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
//     uint32_t inode_block_idx = BASICFTFS_GET_INODE_BLOCK_IDX(ino);

//     bh = sb_bread(sb, inode_block);

//     if (!bh) {
//         iget_failed(inode);
//         return -EIO;
//     }

//     bftfs_inode = (struct basicftfs_inode *) bh->b_data;
//     bftfs_inode += inode_block_idx;

//     write_from_disk_to_vfs_inode(sb, inode, bftfs_inode, ino);
//     init_inode_mode(inode, bftfs_inode, bftfs_inode_info);
//     return 0;
// }

// // static inline int init_empty_dir(struct super_block *sb, struct inode *inode, struct inode *dir) {
// //    int ret = 0;
    
// //    ret = basicftfs_add_entry(inode, inode, ".");

// //    if (ret < 0)return ret;

// //    ret = basicftfs_add_entry(inode, dir, "..");

// //    return ret;
// // }

#endif
#ifndef BASICFTFS_DESTROY_H
#define BASICFTFS_DESTROY_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "bitmap.h"

static inline void reset_block(char *block, struct buffer_head *bh) {
    memset(block, 0, BASICFTFS_BLOCKSIZE);
    mark_buffer_dirty(bh);
    brelse(bh);
}

static inline int clean_block(struct super_block *sb, struct inode *inode) {
    struct buffer_head *bh = NULL;
    char *block = NULL;
    int ret = 0;

    bh = sb_bread(sb, BASICFTFS_INODE(inode)->i_bno);

    if (!bh) return -EIO;

    block = (char *) bh->b_data;
    reset_block(block, bh);
    return ret;
}

static inline void clean_allocated_block(struct basicftfs_alloc_table *alloc_table_block, struct super_block *sb, int bi, int is_allocated) {
    if (is_allocated && alloc_table_block->table[bi] != 0) {
        put_blocks(BASICFTFS_SB(sb), alloc_table_block->table[bi], alloc_table_block->table[bi]);
        alloc_table_block->table[bi] = 0;
    }
}

static inline void clean_inode(struct inode *inode) {
    inode->i_blocks = 0;
    BASICFTFS_INODE(inode)->i_bno = 0;
    inode->i_size = 0;
    i_uid_write(inode, 0);
    i_gid_write(inode, 0);
    inode->i_mode = 0;
    inode->i_ctime.tv_sec = inode->i_mtime.tv_sec = inode->i_atime.tv_sec = 0;
    drop_nlink(inode);
    mark_inode_dirty(inode);
}

#endif
#ifndef BASICFTFS_DESTROY_H
#define BASICFTFS_DESTROY_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"

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

#endif
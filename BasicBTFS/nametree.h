#ifndef BASICBTFS_NAMETREE_H
#define BASICBTFS_NAMETREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>


#include "basicbtfs.h"
#include "bitmap.h"

static inline int basicbtfs_nametree_insert_name(struct super_block *sb, uint32_t name_bno, struct basicbtfs_entry *dir_entry, struct dentry *dentry) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    uint32_t next_bno;
    char *block = NULL;
    char *filename = NULL;
    int i = 0;
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;

    if (name_tree->free_bytes >= dentry->d_name.len) {
        block = (char *) bh->b_data;
        block += (BASICBTFS_BLOCKSIZE - name_tree->free_bytes);
        name_entry = (struct basicbtfs_name_entry *) block;
        name_entry->hash = dir_entry->hash;
        name_entry->name_length = dentry->d_name.len;
        name_tree->free_bytes -= dentry->d_name.len;
        name_entry += 1;
        filename = (char *) name_entry;
        strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
        mark_buffer_dirty(bh);
        brelse(bh);
        return 0;
    }

    while (name_tree->next_block != 0) {
        next_bno = name_tree->next_block;
        brelse(bh);

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        name_tree = (struct basicbtfs_name_tree *) bh->b_data;

        if (name_tree->free_bytes >= dentry->d_name.len) {
            block = (char *) bh->b_data;
            block += (BASICBTFS_BLOCKSIZE - name_tree->free_bytes);
            name_entry = (struct basicbtfs_name_entry *) block;
            name_entry->hash = dir_entry->hash;
            name_entry->name_length = dentry->d_name.len;
            name_tree->free_bytes -= dentry->d_name.len;
            name_entry += 1;
            filename = (char *) name_entry;
            strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
            mark_buffer_dirty(bh);
            brelse(bh);
            return 0;
        }
    }

    name_tree->next_block = get_free_blocks(BASICBTFS_SB(sb), 1);

    next_bno = name_tree->next_block;
    mark_buffer_dirty(bh);
    brelse(bh);

    bh = sb_bread(sb, next_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;
    name_tree->free_bytes = BASICBTFS_EMPTY_NAME_TREE;
    name_tree->next_block = 0;

    if (name_tree->free_bytes >= dentry->d_name.len) {
        block = (char *) bh->b_data;
        block += (BASICBTFS_BLOCKSIZE - name_tree->free_bytes);
        name_entry = (struct basicbtfs_name_entry *) block;
        name_entry->hash = dir_entry->hash;
        name_entry->name_length = dentry->d_name.len;
        name_tree->free_bytes -= dentry->d_name.len;
        name_entry += 1;
        filename = (char *) name_entry;
        strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
        mark_buffer_dirty(bh);
        brelse(bh);
        return 0;
    }
}

#endif
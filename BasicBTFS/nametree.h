#ifndef BASICBTFS_NAMETREE_H
#define BASICBTFS_NAMETREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>


#include "basicbtfs.h"
#include "bitmap.h"

static inline int basicbtfs_nametree_iterate_name(struct super_block *sb, uint32_t name_bno, struct dir_context *ctx, loff_t start_pos) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    uint32_t next_bno;
    char *block = NULL;
    char *filename = NULL;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;
    int i = 0;
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;
    block = (char *) bh->b_data;
    block += sizeof(struct basicbtfs_name_tree);
    cur_entry = (struct basicbtfs_name_entry *) block;
    total_nr_entries = name_tree->nr_of_entries;

    if (start_pos < total_nr_entries) {
        for (i = 0; i < name_tree->nr_of_entries; i++) {
            if (current_index >= start_pos) {
                block += sizeof(struct basicbtfs_name_entry);
                filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
                memcpy(filename, block, cur_entry->name_length);
                if (!dir_emit(ctx, filename, cur_entry->name_length, cur_entry->ino, DT_UNKNOWN)) {
                    kfree(filename);
                    printk(KERN_INFO "No files available anymore\n");
                    break;
                }
                kfree(filename);
            }
            ctx->pos++;
            block += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
            current_index++;
        }
    }

    while (name_tree->next_block != 0) {
        next_bno = name_tree->next_block;
        brelse(bh);

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        name_tree = (struct basicbtfs_name_tree *) bh->b_data;

        total_nr_entries += name_tree->nr_of_entries;
        if (start_pos < total_nr_entries) {
            for (i = 0; i < name_tree->nr_of_entries; i++) {
                if (current_index >= start_pos) {
                    block += sizeof(struct basicbtfs_name_entry);
                    filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
                    memcpy(filename, block, cur_entry->name_length);
                    if (!dir_emit(ctx, filename, cur_entry->name_length, cur_entry->ino, DT_UNKNOWN)) {
                        printk(KERN_INFO "No files available anymore\n");
                        kfree(filename);
                        brelse(bh);
                        goto end;
                    }
                    kfree(filename);
                }

                block += cur_entry->name_length;
                cur_entry = (struct basicbtfs_name_entry *) block;
                current_index++;
            }
        }
    }


    end:
    return 0;
}

static inline int basicbtfs_nametree_insert_name(struct super_block *sb, uint32_t name_bno, struct basicbtfs_entry *dir_entry, struct dentry *dentry) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    uint32_t next_bno;
    char *block = NULL;
    char *filename = NULL;

    printk("hello\n");
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;

    if (name_tree->free_bytes >= dentry->d_name.len) {
        block = (char *) bh->b_data;
        block += (BASICBTFS_BLOCKSIZE - name_tree->free_bytes);
        dir_entry->block_index = BASICBTFS_BLOCKSIZE - name_tree->free_bytes;
        dir_entry->name_bno = name_bno;
        name_entry = (struct basicbtfs_name_entry *) block;
        name_entry->ino = dir_entry->ino;
        name_entry->name_length = dentry->d_name.len + 1;
        name_tree->free_bytes -= (sizeof(struct basicbtfs_name_entry) + dentry->d_name.len + 1);
        name_entry += 1;
        filename = (char *) name_entry;
        strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
        filename[dentry->d_name.len] = '\0';
        printk("inserted filename: %s\n", filename);
        name_tree->nr_of_entries++;
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
            dir_entry->block_index = BASICBTFS_BLOCKSIZE - name_tree->free_bytes;
            dir_entry->name_bno = name_bno;
            name_entry = (struct basicbtfs_name_entry *) block;
            name_entry->ino = dir_entry->ino;
            name_entry->name_length = dentry->d_name.len + 1;
            name_tree->free_bytes -= (sizeof(struct basicbtfs_name_entry) + dentry->d_name.len + 1);
            name_entry += 1;
            filename = (char *) name_entry;
            strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
            filename[dentry->d_name.len] = '\0';
            printk("inserted filename: %s | %x\n", filename, filename[16]);
            name_tree->nr_of_entries++;
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
        dir_entry->block_index = BASICBTFS_BLOCKSIZE - name_tree->free_bytes;
        dir_entry->name_bno = name_bno;
        name_entry = (struct basicbtfs_name_entry *) block;
        name_entry->ino = dir_entry->ino;
        name_entry->name_length = dentry->d_name.len + 1;
        name_tree->free_bytes -= (sizeof(struct basicbtfs_name_entry) + dentry->d_name.len + 1);
        name_entry += 1;
        filename = (char *) name_entry;
        strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
        filename[dentry->d_name.len] = '\0';
        printk("inserted filename: %s | %x\n", filename, filename[16]);
        name_tree->nr_of_entries++;
        mark_buffer_dirty(bh);
        brelse(bh);
        return 0;
    }

    return -1;
}

static inline int basicbtfs_nametree_delete_name(struct super_block *sb, uint32_t name_bno, uint32_t block_index) {
    struct buffer_head *bh = NULL;
    char *block = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    struct basicbtfs_name_tree *name_tree = NULL;
    uint32_t need_to_move = 0, need_to_clear;

    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;
    block = (char *) bh->b_data;
    block += block_index;
    name_entry = (struct basicbtfs_name_entry *) block;
    need_to_move = (BASICBTFS_BLOCKSIZE - name_tree->free_bytes) - block_index;

    memset(block, 0, sizeof(struct basicbtfs_name_entry) + name_entry->name_length);

    memcpy(block, block + sizeof(struct basicbtfs_name_entry) + name_entry->name_length, need_to_move);

    block = (char *) bh->b_data;
    need_to_clear = BASICBTFS_BLOCKSIZE - name_tree->free_bytes - (sizeof(struct basicbtfs_name_entry) + name_entry->name_length);
    memset(block + need_to_clear, 0, sizeof(struct basicbtfs_name_entry) + name_entry->name_length);
    name_tree->nr_of_entries--;

    mark_buffer_dirty(bh);
    brelse(bh);

    return 0;
}

static inline int basicbtfs_nametree_iterate_name_debug(struct super_block *sb, uint32_t name_bno) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    uint32_t next_bno;
    char *block = NULL;
    char *filename = NULL;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;
    int i = 0;
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    name_tree = (struct basicbtfs_name_tree *) bh->b_data;
    block = (char *) bh->b_data;
    block += sizeof(struct basicbtfs_name_tree);
    cur_entry = (struct basicbtfs_name_entry *) block;
    total_nr_entries = name_tree->nr_of_entries;

    for (i = 0; i < name_tree->nr_of_entries; i++) {
        block += sizeof(struct basicbtfs_name_entry);
        filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
        strncpy(filename, block, cur_entry->name_length);
        printk("Current filename: %d | %s\n", cur_entry->name_length, filename);
        kfree(filename);

        block += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
        current_index++;
    }

    while (name_tree->next_block != 0) {
        next_bno = name_tree->next_block;
        brelse(bh);

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        name_tree = (struct basicbtfs_name_tree *) bh->b_data;

        total_nr_entries += name_tree->nr_of_entries;
        for (i = 0; i < name_tree->nr_of_entries; i++) {
            block += sizeof(struct basicbtfs_name_entry);
            filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            printk("Current filename: %d | %s\n", cur_entry->name_length, filename);
            kfree(filename);

            block += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
            current_index++;
        }
    }

    return 0;
}

#endif
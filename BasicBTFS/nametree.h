#ifndef BASICBTFS_NAMETREE_H
#define BASICBTFS_NAMETREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>


#include "basicbtfs.h"
#include "bitmap.h"
#include "cache.h"

static inline int basicbtfs_nametree_emit_block(struct buffer_head *bh, int *total_nr_entries, int *current_index, struct dir_context *ctx, loff_t start_pos) {
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    char *block = NULL;
    char *filename = NULL;
    int i = 0;

    
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    block = (char *) bh->b_data;
    block += BASICBTFS_NAME_ENTRY_S_OFFSET;
    cur_entry = (struct basicbtfs_name_entry *) block;
    *total_nr_entries += name_list_hdr->nr_of_entries;

    if (start_pos < *total_nr_entries) {
        for (i = 0; i < name_list_hdr->nr_of_entries; i++) {
            block += sizeof(struct basicbtfs_name_entry);
            if (cur_entry->ino != 0) {
                filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
                strncpy(filename, block, cur_entry->name_length);
                if (!dir_emit(ctx, filename, cur_entry->name_length - 1, cur_entry->ino, DT_UNKNOWN)) {
                    printk(KERN_INFO "No files available anymore\n");
                    kfree(filename);
                    brelse(bh);
                    return 1;
                }
                ctx->pos++;
                kfree(filename);
            } else {
                i--;
            }

            block += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
            *current_index += 1;
        }
    }
    return 0;
}

static inline int basicbtfs_nametree_iterate_name(struct super_block *sb, uint32_t name_bno, struct dir_context *ctx, loff_t start_pos) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t next_bno;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;
    int ret = 0;
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    ret = basicbtfs_nametree_emit_block(bh, &total_nr_entries, &current_index, ctx, start_pos);

    if (ret == 1) {
        return 0;
    }


    while (name_list_hdr->next_block != 0) {
        next_bno = name_list_hdr->next_block;
        brelse(bh);

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;
        ret = basicbtfs_nametree_emit_block(bh, &total_nr_entries, &current_index, ctx, start_pos);

        if (ret == 1) {
            return 0;
        }
    }
    
    return 0;
}

static inline int basicbtfs_nametree_insert_entry_in_list(struct buffer_head *bh, uint32_t name_bno, struct dentry *dentry, struct basicbtfs_entry *dir_entry ) {
    char *block = NULL, *filename = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    block = (char *) bh->b_data;
    
    block += name_list_hdr->start_unused_area;
    dir_entry->block_index = name_list_hdr->start_unused_area;
    dir_entry->name_bno = name_bno;
    name_entry = (struct basicbtfs_name_entry *) block;
    name_entry->ino = dir_entry->ino;
    name_entry->name_length = dentry->d_name.len + 1;
    name_list_hdr->free_bytes -= (sizeof(struct basicbtfs_name_entry) + dentry->d_name.len + 1);
    name_list_hdr->start_unused_area += (sizeof(struct basicbtfs_name_entry) + dentry->d_name.len + 1);
    name_entry += 1;
    filename = (char *) name_entry;
    strncpy(filename, (char *)dentry->d_name.name, dentry->d_name.len);
    filename[dentry->d_name.len] = '\0';
    printk("inserted filename: %s | %d | %d\n", filename, dir_entry->name_bno, dir_entry->block_index);
    name_list_hdr->nr_of_entries++;
    return 0;
}

static inline int basicbtfs_nametree_insert_name(struct super_block *sb, uint32_t name_bno, struct basicbtfs_entry *dir_entry, struct dentry *dentry, uint32_t dir_bno, uint32_t nr_of_blocks) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t cur_bno = name_bno, prev_bno = cur_bno;
    
    bh = sb_bread(sb, cur_bno);

    if (!bh) return -EIO;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    if ((BASICBTFS_BLOCKSIZE - name_list_hdr->start_unused_area) > dentry->d_name.len + 1 + sizeof(struct basicbtfs_name_entry)) {
        basicbtfs_nametree_insert_entry_in_list(bh, cur_bno, dentry, dir_entry);
        mark_buffer_dirty(bh);
        basicbtfs_cache_update_block(sb, dir_bno, (struct basicbtfs_block *) bh->b_data, cur_bno);
        brelse(bh);
        return 0;
    }

    printk("not enough space: %d\n", (BASICBTFS_BLOCKSIZE - name_list_hdr->start_unused_area));

    while (name_list_hdr->next_block != 0) {
        prev_bno = cur_bno;
        cur_bno = name_list_hdr->next_block;
        brelse(bh);

        bh = sb_bread(sb, cur_bno);

        if (!bh) return -EIO;

        // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;

        if ((BASICBTFS_BLOCKSIZE - name_list_hdr->start_unused_area) > dentry->d_name.len + 1 + sizeof(struct basicbtfs_name_entry)) {
            basicbtfs_nametree_insert_entry_in_list(bh, cur_bno, dentry, dir_entry);
            mark_buffer_dirty(bh);
            basicbtfs_cache_update_block(sb, dir_bno, (struct basicbtfs_block *) bh->b_data, cur_bno);
            brelse(bh);
            return 0;
        }
    }

    name_list_hdr->next_block = get_free_blocks(BASICBTFS_SB(sb), 1);

    cur_bno = name_list_hdr->next_block;
    mark_buffer_dirty(bh);
    brelse(bh);

    bh = sb_bread(sb, cur_bno);

    if (!bh) return -EIO;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    memset(disk_block, 0, sizeof(struct basicbtfs_disk_block));
    disk_block->block_type_id = BASICBTFS_BLOCKTYPE_NAMETREE;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    name_list_hdr->free_bytes = BASICBTFS_EMPTY_NAME_TREE;
    name_list_hdr->start_unused_area = BASICBTFS_BLOCKSIZE - BASICBTFS_EMPTY_NAME_TREE;
    // name_list_hdr->block_type = BASICBTFS_BLOCKTYPE_NAMETREE;
    name_list_hdr->prev_block = prev_bno;
    name_list_hdr->next_block = 0;
    name_list_hdr->nr_of_entries = 0;

    if ((BASICBTFS_BLOCKSIZE - name_list_hdr->start_unused_area) > dentry->d_name.len + 1 +  sizeof(struct basicbtfs_name_entry)) {
        basicbtfs_nametree_insert_entry_in_list(bh, cur_bno, dentry, dir_entry);
        mark_buffer_dirty(bh);
        
        if (nr_of_blocks < BASICBTFS_MAX_CACHE_BLOCKS_PER_DIR) {
            basicbtfs_cache_add_name_block(sb, dir_bno, (struct basicbtfs_block *)bh->b_data, cur_bno);
        }
        brelse(bh);
        return 0;
    }

    brelse(bh);
    return -1;
}

static inline int basicbtfs_nametree_delete_name(struct super_block *sb, uint32_t name_bno, uint32_t block_index, uint32_t dir_bno) {
    struct buffer_head *bh = NULL;
    char *block = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t old_free_bytes = 0;

    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    block = (char *) bh->b_data;
    block += block_index;
    name_entry = (struct basicbtfs_name_entry *) block;
    name_entry->ino = 0;
    old_free_bytes = name_list_hdr->free_bytes;
    name_list_hdr->free_bytes += (sizeof(struct basicbtfs_name_entry) + name_entry->name_length);

    block += sizeof(struct basicbtfs_name_entry);
    // memset(block, 0, name_entry->name_length);

    name_list_hdr->nr_of_entries--;

    mark_buffer_dirty(bh);
    basicbtfs_cache_update_block(sb, dir_bno, (struct basicbtfs_block *)bh->b_data, name_bno);
    brelse(bh);

    return 0;
}

static inline int basicbtfs_nametree_iterate_name_debug(struct super_block *sb, uint32_t name_bno) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    uint32_t next_bno;
    char *block = NULL;
    char *filename = NULL;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;
    int i = 0;

    printk("name debug\n");
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    printk("1\n");
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    printk("12\n");
    block = bh->b_data;
    block += BASICBTFS_NAME_ENTRY_S_OFFSET;
    cur_entry = (struct basicbtfs_name_entry *) block;
    printk("13\n");
    total_nr_entries = name_list_hdr->nr_of_entries;

    printk("done: %ld\n", BASICBTFS_NAME_ENTRY_S_OFFSET + sizeof(struct basicbtfs_name_entry));

    for (i = 0; i < name_list_hdr->nr_of_entries; i++) {
        block += sizeof(struct basicbtfs_name_entry);
        if (cur_entry->ino != 0) {
            filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            printk("Current filename: %d | %d | %s\n", cur_entry->name_length, i, filename);
            kfree(filename);
        } else {
            i--;
        }

        block += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
        current_index++;
    }

    while (name_list_hdr->next_block != 0) {
        next_bno = name_list_hdr->next_block;
        brelse(bh);

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;
        block = bh->b_data;
        block += BASICBTFS_NAME_ENTRY_S_OFFSET;
        cur_entry = (struct basicbtfs_name_entry *) block;
        total_nr_entries += name_list_hdr->nr_of_entries;

        for (i = 0; i < name_list_hdr->nr_of_entries; i++) {
            printk("next entry: %d\n", i);
            block += sizeof(struct basicbtfs_name_entry);
            if (cur_entry->ino != 0) {
                filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
                strncpy(filename, block, cur_entry->name_length);
                printk("Current filename: %d | %d | %s\n", cur_entry->name_length, current_index, filename);
                kfree(filename);
            } else {
                i--;
            }

            block += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
            current_index++;
        }
    }

    return 0;
}

#endif
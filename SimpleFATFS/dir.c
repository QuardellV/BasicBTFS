
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "bitmap.h"
#include "destroy.h"

// TODO: Update address directly
static int basicftfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicftfs_inode_info *inode_info =BASICFTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh_table = NULL, *bh_block = NULL;
    struct basicftfs_alloc_table *alloc_table = NULL;
    struct basicftfs_entry_list *entry_list = NULL;
    struct basicftfs_entry *entry = NULL;
    uint32_t block_idx = 0, entry_idx = 0;

    if (!S_ISDIR(inode->i_mode)) {
        printk(KERN_ERR "No such directory exists\n");
        return -ENOTDIR;
    }

    if (ctx->pos > BASICFTFS_ENTRIES_PER_DIR) {
        printk(KERN_ERR "No such ctx position should exist\n");
        return 0;
    }

    bh_table = sb_bread(sb, inode_info->i_bno);

    if (!bh_table) return -EIO;

    alloc_table = (struct basicftfs_alloc_table *) bh_table->b_data;

    block_idx = BASICFTFS_GET_BLOCK_IDX(ctx->pos);
    entry_idx = BASICFTFS_GET_ENTRY_IDX(ctx->pos);

    while (block_idx < BASICFTFS_ENTRIES_PER_BLOCK && alloc_table->table[block_idx] != 0) {
        bh_block = sb_bread(sb, alloc_table->table[block_idx]);

        if (!bh_block) {
            brelse(bh_table);
            return -EIO;
        }

        entry_list = (struct basicftfs_entry_list *) bh_block->b_data;

        if (entry_list->entries[0].ino == 0) break;

        for (; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            entry = &entry_list->entries[entry_idx];

            if (entry->ino && !dir_emit(ctx, entry->hash_name, BASICFTFS_NAME_LENGTH, entry->ino, DT_UNKNOWN)) {
                break;
            }
            ctx->pos++;
        }
        brelse(bh_block);
        bh_block = NULL;
        block_idx++;
        entry_idx = 0;
    }
    brelse(bh_table);
    return 0;
}

int basicftfs_add_entry(struct inode *dir, struct inode *inode, const unsigned char *name) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *dir_inode_info = BASICFTFS_INODE(dir);
    struct buffer_head *bh_dir, *bh_new = NULL;
    struct basicftfs_alloc_table * alloc_table_block = NULL;
    struct basicftfs_entry_list *entry_list = NULL;
    uint32_t block_idx = 0, entry_idx = 0, bno = 0;
    bool is_allocated = false;

    bh_dir = sb_bread(sb, dir_inode_info->i_bno);

    if (!bh_dir) return -EIO;

    alloc_table_block = (struct basicftfs_alloc_table *) bh_dir->b_data;
    block_idx = BASICFTFS_GET_BLOCK_IDX((alloc_table_block->nr_of_entries));
    entry_idx = BASICFTFS_GET_ENTRY_IDX((alloc_table_block->nr_of_entries));

    if (alloc_table_block->table[block_idx] == 0) {
        bno = get_free_blocks(BASICFTFS_SB(sb), 1);

        if (!bno) {
            put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
            // dir->i_blocks--;
            put_inode(BASICFTFS_SB(sb), inode->i_ino);
            iput(inode);
            brelse(bh_dir);
            return -ENOSPC;
        }
        alloc_table_block->table[block_idx] = bno;
        is_allocated = true;
    }

    bh_new = sb_bread(sb, alloc_table_block->table[block_idx]);

    if (!bh_new) {
        clean_allocated_block(alloc_table_block, sb, block_idx, is_allocated);
        put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
        // dir->i_blocks--;
        put_inode(BASICFTFS_SB(sb), inode->i_ino);
        iput(inode);
        brelse(bh_dir);
        return -ENOSPC;
    }

    entry_list = (struct basicftfs_entry_list *) bh_new->b_data;
    entry_list->entries[entry_idx].ino = inode->i_ino;
    strncpy(entry_list->entries[entry_idx].hash_name, name, BASICFTFS_NAME_LENGTH);

    alloc_table_block->nr_of_entries++;
    mark_buffer_dirty(bh_new);
    mark_buffer_dirty(bh_dir);
    brelse(bh_new);
    brelse(bh_dir);
    return 0;
}

struct dentry *basicftfs_search_entry(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *dir_info = BASICFTFS_INODE(dir);
    struct buffer_head *bh_table = NULL, *bh_block = NULL;
    struct basicftfs_alloc_table * alloc_table_block = NULL;
    struct basicftfs_entry_list *entry_list = NULL;
    struct basicftfs_entry *entry = NULL;
    struct inode *inode = NULL;
    uint32_t block_idx = 0, entry_idx = 0;

    bh_table = sb_bread(sb, dir_info->i_bno);

    if (!bh_table) return ERR_PTR(-EIO);

    alloc_table_block = (struct basicftfs_alloc_table *) bh_table->b_data;

    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && alloc_table_block->table[block_idx] != 0) {
        bh_block = sb_bread(sb, alloc_table_block->table[block_idx]);

        if (!bh_block) {
            brelse(bh_table);
            return ERR_PTR(-EIO);
        }

        entry_list = (struct basicftfs_entry_list *) bh_block->b_data;

        for (entry_idx = 0; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            entry = &entry_list->entries[entry_idx];

            if (entry->ino == 0) {
                brelse(bh_block);
                goto end;
            }

            if (strncmp(entry->hash_name, dentry->d_name.name, BASICFTFS_NAME_LENGTH) == 0) {
                inode = basicftfs_iget(sb, entry->ino);
                brelse(bh_block);
                goto end;
            }
        }
        brelse(bh_block);
        bh_block = NULL;
        block_idx++;
    }

end:
    brelse(bh_table);
    dir->i_atime = current_time(dir);
    mark_inode_dirty(dir);
    d_add(dentry, inode);
    return NULL;

    return NULL;
}

const struct file_operations basicftfs_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = basicftfs_iterate,
};
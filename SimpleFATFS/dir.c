
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "bitmap.h"
#include "destroy.h"

static int basicfs_iterate(struct file *dir, struct dir_context *ctx) {
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

const struct file_operations basicftfs_dir_ops = {
    // .owner = THIS_MODULE,
    .iterate_shared = basicftfs_iterate,
};
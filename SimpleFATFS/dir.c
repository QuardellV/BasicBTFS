#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "destroy.h"

// TODO: Update address directly
static int basicftfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicftfs_inode_info *inode_info = BASICFTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh_block = NULL, *bh_dir = NULL;
    struct basicftfs_alloc_table *root_block = NULL;
    struct basicftfs_entry_list *fblock = NULL;
    struct basicftfs_entry *f = NULL;
    int  block_idx = 0, entry_idx = 0;
    int ret = 0;

    if (!S_ISDIR(inode->i_mode)) {
        printk(KERN_ERR "This file is not a directory\n");
        return -ENOTDIR;
    }

    if (ctx->pos > BASICFTFS_ENTRIES_PER_DIR + 2) {
        printk(KERN_ERR "ctx position is bigger than the amount of subfiles we can handle in a directory\n");
        return 0;
    }

    // fix if two entries are added. However, currently are not being displayed with ls :(
    if (!dir_emit_dots(dir, ctx)) return 0;

    printk("basicftfs_iterate() sb_bread inode_info->data_block: %d\n", inode_info->i_bno);
    bh_block = sb_bread(sb, inode_info->i_bno);
    if (!bh_block) {
        return -EIO;
    }

    root_block = (struct basicftfs_alloc_table *) bh_block->b_data;
    block_idx = BASICFTFS_GET_BLOCK_IDX((ctx->pos - 2));
    entry_idx = BASICFTFS_GET_ENTRY_IDX((ctx->pos - 2));


    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && root_block->table[block_idx] != 0) {
        bh_dir = sb_bread(sb, root_block->table[block_idx]);

        if (!bh_dir) {
            brelse(bh_block);
            return -EIO;
        }

        fblock = (struct basicftfs_entry_list *) bh_dir->b_data;
        if (fblock->entries[0].ino == 0) break;

        for (; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            f = &fblock->entries[entry_idx];

            if (f->ino && !dir_emit(ctx, f->hash_name, BASICFTFS_NAME_LENGTH, f->ino, DT_UNKNOWN)) {
                // printk(KERN_INFO "No files available anymore\n");
                break;
            }
            ctx->pos++;
        }
        brelse(bh_dir);
        bh_dir = NULL;
        entry_idx = 0;
        block_idx++;
    }

    brelse(bh_block);
    return ret;
}

struct dentry *basicftfs_search_entry(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *ci_dir = BASICFTFS_INODE(dir);
    struct inode *inode = NULL;
    struct buffer_head *bh_block = NULL, *bh_clusters = NULL;
    struct basicftfs_alloc_table *cblock = NULL;
    struct basicftfs_entry_list *fblock = NULL;
    struct basicftfs_entry *f = NULL;
    int block_idx = 0, entry_idx = 0;
    uint32_t cur_block = 0;

    printk("basicftfs_lookup() sb_bread ci_dir->data_block: %d\n", ci_dir->i_bno);
    bh_clusters = sb_bread(sb, ci_dir->i_bno);
    if (!bh_clusters) {
        return ERR_PTR(-EIO);
    }

    cblock = (struct basicftfs_alloc_table *) bh_clusters->b_data;

    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && cblock->table[block_idx] != 0) {
        cur_block = cblock->table[block_idx];
        printk("basicftfs_lookup() sb_bread cur_page: %d\n", cur_block);
        bh_block = sb_bread(sb, cur_block);

        if (!bh_block) {
            return ERR_PTR(-EIO);
        }

        fblock = (struct basicftfs_entry_list *) bh_block->b_data;

        for (entry_idx = 0; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            f = &fblock->entries[entry_idx];
            if (f->ino == 0) {
                brelse(bh_block);
                goto lookup_end;
            }
            if (strncmp(f->hash_name, dentry->d_name.name, BASICFTFS_NAME_LENGTH) == 0) {
                inode = basicftfs_iget(sb, f->ino);
                brelse(bh_block);
                goto lookup_end;
            }
        }
        brelse(bh_block);
        bh_block = NULL;
        block_idx++;
    }


lookup_end:
    brelse(bh_clusters);

    dir->i_atime = current_time(dir);
    mark_inode_dirty(dir);
    d_add(dentry, inode);
    return NULL;
}

int basicftfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *bfs_dir = BASICFTFS_INODE(dir);
    struct basicftfs_alloc_table *cblock = NULL;
    struct basicftfs_entry_list *fblock = NULL;
    struct buffer_head *bh_dir, *bh_dblock = NULL;
    uint32_t bno = 0;
    int ret = 0, is_allocated = false;
    int block_idx = 0, entry_idx = 0;

    printk("basicftfs_link() sb_bread ci_dir->data_block: %d\n", bfs_dir->i_bno);
    bh_dir = sb_bread(sb, bfs_dir->i_bno);

    if (!bh_dir) {
        return -EIO;
    }

    cblock = (struct basicftfs_alloc_table *) bh_dir->b_data;

    block_idx = BASICFTFS_GET_BLOCK_IDX((cblock->nr_of_entries));
    entry_idx = BASICFTFS_GET_ENTRY_IDX((cblock->nr_of_entries));

    printk("basicftfs_create() sb_bread bi | fi: %d | %d\n", block_idx, entry_idx);

    if (cblock->table[block_idx] == 0) {
        bno = get_free_blocks(BASICFTFS_SB(sb), 1);
        if (!bno) {
            printk(KERN_ERR "No free block available\n");
            ret = -ENOSPC;
            goto clean_allocated_inode;
        }
        cblock->table[block_idx] = bno;
        // dir->i_blocks += 1;
        is_allocated = true;
    }

    printk("basicftfs_create() sb_bread cblock->blocks[bi]: %d\n", cblock->table[block_idx]);
    bh_dblock = sb_bread(sb, cblock->table[block_idx]);
    if (!bh_dblock) {
        ret = -EIO;
        goto clean_allocated_dir_block;
    }

    fblock = (struct basicftfs_entry_list *) bh_dblock->b_data;

    fblock->entries[entry_idx].ino = inode->i_ino;
    strncpy(fblock->entries[entry_idx].hash_name, dentry->d_name.name, BASICFTFS_NAME_LENGTH);

    cblock->nr_of_entries++;
    printk("nr of files after creation: %d\n", cblock->nr_of_entries);
    mark_buffer_dirty(bh_dblock);
    mark_buffer_dirty(bh_dir);
    brelse(bh_dblock);
    brelse(bh_dir);
    return 0;

clean_allocated_dir_block:
    clean_allocated_block(cblock, sb, block_idx, is_allocated);
    // dir->i_blocks -= 1;

clean_allocated_inode:
    put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
    // dir->i_blocks--;
    put_inode(BASICFTFS_SB(sb), inode->i_ino);
    iput(inode);

    brelse(bh_dir);
    return ret;
}

int clean_file_block(struct inode *inode) {
    struct super_block *sb = inode->i_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    uint32_t bno = 0;
    int bi = 0;
    struct buffer_head *bh = NULL, *bh2 = NULL;

    struct basicftfs_alloc_table *file_block = NULL;

    bh = sb_bread(sb, BASICFTFS_INODE(inode)->i_bno);
    if (!bh) {
        return -EMLINK;
    }

    file_block = (struct basicftfs_alloc_table *) bh->b_data;

    while (bi < BASICFTFS_ATABLE_MAX_BLOCKS && file_block->table[bi] != 0) {
        char *block;

        put_blocks(sbi, file_block->table[bi], 1);
        // dir->i_blocks -= file_block->clusters[ci].nr_of_blocks;


        bno = file_block->table[bi];
        printk("basicftfs_unlink() sb_bread page: %d\n", bno);
        bh2 = sb_bread(sb, bno);
        if (!bh2) {
            continue;
        }

        block = (char *) bh2->b_data;
        reset_block(block, bh2);
        bi++;
    }

    brelse(bh);
    return 0;
}

const struct file_operations basicftfs_dir_ops = {
    .owner = THIS_MODULE,
    .iterate_shared = basicftfs_iterate,
};
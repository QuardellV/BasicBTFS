#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"

static int basicftfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicftfs_inode_info *inode_info = BASICFTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh_block = NULL, *bh_dir = NULL;
    struct basicftfs_alloc_table *root_block = NULL;
    struct basicftfs_entry *entry = NULL;
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

    // Tried different approaches to show the . However, currently are not being displayed with ls for the root inode :(
    if (!dir_emit_dots(dir, ctx)) return 0;

    bh_block = sb_bread(sb, inode_info->i_bno);
    if (!bh_block) {
        return -EIO;
    }

    root_block = (struct basicftfs_alloc_table *) bh_block->b_data;

    if (ret < 0) {
        brelse(bh_block);
        return ret;
    }

    block_idx = BASICFTFS_GET_BLOCK_IDX((ctx->pos - 2));
    entry_idx = BASICFTFS_GET_ENTRY_IDX((ctx->pos  -2));


    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && root_block->table[block_idx] != 0) {
        bh_dir = sb_bread(sb, root_block->table[block_idx]);

        if (!bh_dir) {
            brelse(bh_block);
            return -EIO;
        }

        entry = (struct basicftfs_entry *) bh_dir->b_data;
        if (entry->ino == 0) break;

        for (; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            if (entry->ino && !dir_emit(ctx, entry->hash_name, BASICFTFS_NAME_LENGTH, entry->ino, DT_UNKNOWN)) {
                break;
            }
            ctx->pos++;
            entry++;
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
    struct basicftfs_entry *entry = NULL;
    int block_idx = 0, entry_idx = 0;
    uint32_t cur_block = 0;

    bh_clusters = sb_bread(sb, ci_dir->i_bno);
    if (!bh_clusters) {
        return ERR_PTR(-EIO);
    }

    cblock = (struct basicftfs_alloc_table *) bh_clusters->b_data;

    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && cblock->table[block_idx] != 0) {
        cur_block = cblock->table[block_idx];
        bh_block = sb_bread(sb, cur_block);

        if (!bh_block) {
            return ERR_PTR(-EIO);
        }

        entry = (struct basicftfs_entry *) bh_block->b_data;

        for (entry_idx = 0; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            if (entry->ino == 0) {
                brelse(bh_block);
                goto lookup_end;
            }
            if (strncmp(entry->hash_name, dentry->d_name.name, BASICFTFS_NAME_LENGTH) == 0) {
                inode = basicftfs_iget(sb, entry->ino);
                brelse(bh_block);
                goto lookup_end;
            }
            entry++;
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

int basicftfs_add_entry_name(struct inode *dir, struct inode *inode, char *filename) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *bfs_dir = BASICFTFS_INODE(dir);
    struct basicftfs_alloc_table *cblock = NULL;
    struct basicftfs_entry *entry = NULL;
    struct buffer_head *bh_dir, *bh_dblock = NULL;
    uint32_t bno = 0;
    int ret = 0, is_allocated = false;
    int block_idx = 0, entry_idx = 0;

    bh_dir = sb_bread(sb, bfs_dir->i_bno);

    if (!bh_dir) {
        return -EIO;
    }

    cblock = (struct basicftfs_alloc_table *) bh_dir->b_data;

    block_idx = BASICFTFS_GET_BLOCK_IDX((cblock->nr_of_entries));
    entry_idx = BASICFTFS_GET_ENTRY_IDX((cblock->nr_of_entries));

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

    bh_dblock = sb_bread(sb, cblock->table[block_idx]);
    if (!bh_dblock) {
        ret = -EIO;
        goto clean_allocated_dir_block;
    }

    entry = (struct basicftfs_entry *) bh_dblock->b_data;
    entry += entry_idx;
    entry->ino = inode->i_ino;
    strncpy(entry->hash_name, filename, BASICFTFS_NAME_LENGTH);

    cblock->nr_of_entries++;
    mark_buffer_dirty(bh_dblock);
    mark_buffer_dirty(bh_dir);
    brelse(bh_dblock);
    brelse(bh_dir);
    return 0;

clean_allocated_dir_block:
    clean_allocated_block(cblock, sb, block_idx, is_allocated);

clean_allocated_inode:
    put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
    put_inode(BASICFTFS_SB(sb), inode->i_ino);
    iput(inode);

    brelse(bh_dir);
    return ret;
}

int basicftfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry) {
    return basicftfs_add_entry_name(dir, inode, (char *)dentry->d_name.name);
}

int basicftfs_delete_entry(struct inode *dir, struct inode *inode) {
    struct super_block *sb = dir->i_sb;
    struct buffer_head *bh = NULL, *bh2 = NULL, *bh_prev = NULL;
    struct basicftfs_alloc_table *cblock = NULL;
    struct basicftfs_entry *entry_cur_saddr = NULL,*entry_cur = NULL, *entry_prev = NULL;
    int bi = 0, fi = 0;
    int ret = 0, has_found = false;
    
    bh = sb_bread(sb, BASICFTFS_INODE(dir)->i_bno);
    if (!bh) {
        return -EIO;
    }

    cblock = (struct basicftfs_alloc_table *) bh->b_data;
    while (bi < BASICFTFS_ATABLE_MAX_BLOCKS && cblock->table[bi] != 0) {
        uint32_t page = cblock->table[bi];
        bh2 = sb_bread(sb, page);
        if (!bh2) {
            brelse(bh);
            return -EIO;
        }
        entry_cur_saddr = (struct basicftfs_entry *) bh2->b_data;
        entry_cur = entry_cur_saddr;

        if (entry_cur->ino == 0) {
            break;
        }

        if (has_found) {
            move_files(entry_prev, entry_cur_saddr);
            mark_buffer_dirty(bh2);
            brelse(bh_prev);
            bh_prev = bh2;
            entry_prev = entry_cur_saddr;
        } else {
            for (fi = 0; fi < BASICFTFS_ENTRIES_PER_BLOCK; fi++) {
                if (entry_cur->ino == inode->i_ino) {
                    has_found = true;
                    remove_file_from_dir(fi, entry_cur_saddr);
                    mark_buffer_dirty(bh2);
                    bh_prev = bh2;
                    entry_prev = entry_cur_saddr;
                    break;
                }
                entry_cur++;
            }
        }

        if (!has_found) {
            brelse(bh2);
        }
        bi++;
    }

    if (has_found) {
        if (bh_prev) {
            brelse(bh_prev);
        }
        cblock->nr_of_entries--;
        mark_buffer_dirty(bh);
    }

    brelse(bh);
    return ret;
}

int basicftfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags) {
    struct super_block *sb = old_dir->i_sb;
    struct buffer_head *bh_new_dir = NULL, *bh_block = NULL;
    struct basicftfs_inode_info *new_dir_info = BASICFTFS_INODE(new_dir);
    struct inode *old_inode = d_inode(old_dentry);
    struct inode *new_inode = d_inode(new_dentry);
    struct basicftfs_alloc_table *a_table = NULL;
    struct basicftfs_entry *entry = NULL;
    int block_idx = 0, entry_idx = 0, ret = 0;
    uint32_t cur_block = 0;

    if (flags & (RENAME_WHITEOUT)) {
        return -EINVAL;
    }

    if (flags & (RENAME_EXCHANGE)) {
        if (!(old_inode && new_inode)) {
            return -EINVAL;
        }
    }

    bh_new_dir = sb_bread(sb, new_dir_info->i_bno);

    if (!bh_new_dir) return -EIO;


    a_table = (struct basicftfs_alloc_table *) bh_new_dir->b_data;

    while (block_idx < BASICFTFS_ATABLE_MAX_BLOCKS && a_table->table[block_idx] != 0) {
        cur_block = a_table->table[block_idx];
        bh_block = sb_bread(sb, cur_block);

        if (!bh_block) {
            return -EIO;
        }

        entry = (struct basicftfs_entry *) bh_block->b_data;

        for (entry_idx = 0; entry_idx < BASICFTFS_ENTRIES_PER_BLOCK; entry_idx++) {
            if (entry->ino == 0) {
                brelse(bh_block);
                goto iterate_end;
            }

            if (strncmp(entry->hash_name, new_dentry->d_name.name, BASICFTFS_NAME_LENGTH) == 0) {
                if (flags & (RENAME_NOREPLACE) && new_dir == old_dir) {
                    ret = -EEXIST;
                    brelse(bh_block);
                    goto end;
                } else {
                    if (new_inode) {
                        // Is it necessary to delete old entry, since it might get referenced to other things
                        ret = basicftfs_delete_entry(new_dir, new_inode);
                    }

                    entry->ino = old_inode->i_ino;
                    strncpy(entry->hash_name, new_dentry->d_name.name, BASICFTFS_NAME_LENGTH);
                    mark_buffer_dirty(bh_block);
                    brelse(bh_block);
                    ret = basicftfs_delete_entry(old_dir, old_inode);
                    goto end;
                }
            }
            entry++;
        }
        brelse(bh_block);
        bh_block = NULL;
        block_idx++;
    }

    iterate_end:
    if (a_table->nr_of_entries >= BASICFTFS_ENTRIES_PER_DIR) {
        return -EMLINK;
    }

    brelse(bh_new_dir);

    ret = basicftfs_add_entry(new_dir, old_inode, new_dentry);

    if (ret < 0) return ret;
    
    ret = basicftfs_delete_entry(old_dir, old_inode);

    return ret;

    end:
    brelse(bh_new_dir);
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

        bno = file_block->table[bi];
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
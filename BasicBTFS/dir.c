#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kmod.h>

#include "basicbtfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"
#include "btree.h"
#include "btreecache.h"
#include "nametree.h"
#include "cache.h"
#include "defrag.h"

static int basicbtfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    uint32_t name_bno = 0, nr_of_files = 0;
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *node = NULL;

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    if (!S_ISDIR(inode->i_mode)) {
        printk(KERN_ERR "This file is not a directory\n");
        return -ENOTDIR;
    }

    if (ctx->pos > BASICBTFS_ENTRIES_PER_DIR + 2) {
        printk(KERN_ERR "ctx position is bigger than the amount of subfiles we can handle in a directory\n");
        return 0;
    }

    if (!dir_emit_dots(dir, ctx)) {
        return 0;
    }

    bh = sb_bread(sb, inode_info->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    name_bno = node->tree_name_bno;
    nr_of_files = node->nr_of_files;
    brelse(bh);

    if (basicbtfs_cache_iterate_dir(sb, inode_info->i_bno, ctx, ctx->pos - 2)) {
        if (ctx->pos - 2 >= nr_of_files) {
            return 0;
        }
    }

    return basicbtfs_nametree_iterate_name(sb, name_bno, ctx, ctx->pos - 2);
}

struct dentry *basicbtfs_search_entry(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    struct inode *inode = NULL;
    uint32_t ino = 0, hash = 0;

    hash = get_hash_from_block((char *)dentry->d_name.name, dentry->d_name.len);

    ino = basicbtfs_cache_lookup_entry(sb, inode_info->i_bno, hash);

    if (ino != 0 && ino != -1) {
        inode = basicbtfs_iget(sb, ino);
        goto end;
    }

    ino = basicbtfs_btree_node_lookup(sb, inode_info->i_bno, hash, 0);

    if (ino != 0 && ino != -1) {
        inode = basicbtfs_iget(sb, ino);
    }

    end:
    dir->i_atime = current_time(dir);
    d_add(dentry, inode);
    return NULL;
}

int basicbtfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry) {
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    int ret = 0;
    struct basicbtfs_entry new_entry;
    uint32_t name_bno = 0, hash = 0, nr_of_blocks = 0;
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node_cache *node_cache = NULL;

    bh = sb_bread(dir->i_sb, inode_info->i_bno);
    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    name_bno = node->tree_name_bno;
    brelse(bh);

    hash = get_hash_from_block((char *)dentry->d_name.name, dentry->d_name.len);

    ret = basicbtfs_btree_node_lookup(dir->i_sb, inode_info->i_bno, hash, 0);

    if (ret != -1 && ret > 0) {
        printk(KERN_INFO "Filename %s already exists\n", dentry->d_name.name);
        return -EIO;
    }

    new_entry.ino = inode->i_ino;
    new_entry.hash = hash;

    nr_of_blocks = basicbtfs_cache_get_nr_of_blocks(inode_info->i_bno);


    ret = basicbtfs_nametree_insert_name(dir->i_sb, name_bno, &new_entry, dentry, inode_info->i_bno, nr_of_blocks);

    ret = basicbtfs_btree_node_insert(dir->i_sb, dir, inode_info->i_bno, &new_entry);

    if (nr_of_blocks < BASICBTFS_MAX_CACHE_BLOCKS_PER_DIR) {
        node_cache = basicbtfs_cache_get_root_node(inode_info->i_bno);
        if (!node_cache) return ret;
        ret = basicbtfs_btree_node_cache_insert(dir->i_sb, dir, node_cache, &new_entry, inode_info->i_bno);
    }

    return ret;
}

int basicbtfs_delete_entry(struct inode *dir, struct dentry *dentry) {
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    int ret = 0;
    uint32_t name_bno = 0, ino = 0;
    uint32_t hash = 0;
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_btree_node_cache *node_cache = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_entry new_entry;

    bh = sb_bread(dir->i_sb, inode_info->i_bno);
    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    name_bno = node->tree_name_bno;
    brelse(bh);

    hash = get_hash_from_block((char *)dentry->d_name.name, dentry->d_name.len);

    ino = basicbtfs_btree_node_lookup_with_entry(dir->i_sb, inode_info->i_bno, hash, 0, &new_entry);

    node_cache = basicbtfs_cache_get_root_node(inode_info->i_bno);
    if (node_cache) {
        ino = basicbtfs_btree_node_cache_lookup(node_cache, hash, 0);
    }

    ret = basicbtfs_btree_delete_entry(dir->i_sb, dir, inode_info->i_bno, hash);
    
    if (node_cache) {
        ret = basicbtfs_btree_cache_delete_entry(dir->i_sb, dir, node_cache, hash, inode_info->i_bno);
    }

    ret = basicbtfs_nametree_delete_name(dir->i_sb, new_entry.name_bno, new_entry.block_index, inode_info->i_bno);

    return ret;
}

int basicbtfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags) {
    struct super_block *sb = old_dir->i_sb;
    struct inode *old_inode = d_inode(old_dentry);
    struct inode *new_inode = d_inode(new_dentry);
    struct basicbtfs_inode_info *new_dir_info = BASICBTFS_INODE(new_dir);
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node * node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t hash = 0;
    int ret = 0;

    if (flags & (RENAME_WHITEOUT | RENAME_NOREPLACE)) {
        return -EINVAL;
    }

    if (flags & (RENAME_EXCHANGE)) {
        if (!(old_inode && new_inode)) {
            return -EINVAL;
        }
    }

    hash = get_hash_from_block((char *)new_dentry->d_name.name, new_dentry->d_name.len);

    ret = basicbtfs_btree_node_lookup(sb, new_dir_info->i_bno, hash, 0);

    if (ret != -1 && ret > 0) {
        return -EEXIST;
    }

    bh = sb_bread(sb, new_dir_info->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    if (node->nr_of_files >= BASICBTFS_ENTRIES_PER_DIR) {
        brelse(bh);
        return -EMLINK;
    }

    brelse(bh);


    ret = basicbtfs_add_entry(new_dir, old_inode, new_dentry);

    if (ret < 0) return ret;

    ret = basicbtfs_delete_entry(old_dir, old_dentry);
    return ret;
}

int basicbtfs_btree_free_dir(struct super_block *sb, struct inode *inode, uint32_t bno) {
    /**
     * 1. If root, move btree and update inode->i_bno on disk and cache
     * 2. if non-root, move btree and update parent reference
     */

    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    int index = 0, ret = 0;

    if (bno == 0 || bno > sbi->s_nblocks) {
        return -1;
    }

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_btree_free_dir(sb, inode, node->children[index]);

            if (ret != 0 || ret == -1) {
                brelse(bh);
                return ret;
            }
        }
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_free_dir(sb, inode, node->children[index]);
        if (ret == -1) return ret;
    }

    put_blocks(sbi, bno, 1);
    brelse(bh);
    return 0;
}

int basicbtfs_nametree_free_namelist_blocks(struct super_block *sb, uint32_t name_bno) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    uint32_t next_bno, cur_bno = name_bno;
    
    bh = sb_bread(sb, name_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;

    while (name_list_hdr->next_block != 0) {
        next_bno = name_list_hdr->next_block;
        put_blocks(sbi, cur_bno, 1);
        brelse(bh);

        cur_bno = next_bno;

        bh = sb_bread(sb, next_bno);

        if (!bh) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;
    }

    put_blocks(sbi, cur_bno, 1);
    brelse(bh);
    return 0;
}

int clean_file_block(struct inode *inode) {
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    uint32_t bno = 0;
    int bi = 0;
    struct buffer_head *bh = NULL, *bh2 = NULL;

    struct basicbtfs_alloc_table *file_block = NULL;

    bh = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);
    if (!bh) {
        return -EMLINK;
    }

    file_block = (struct basicbtfs_alloc_table *) bh->b_data;

    while (bi < BASICBTFS_ATABLE_MAX_CLUSTERS && file_block->table[bi] != 0) {
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

const struct file_operations basicbtfs_dir_ops = {
    .owner = THIS_MODULE,
    .llseek		= generic_file_llseek,
	.read		= generic_read_dir,
    .iterate_shared = basicbtfs_iterate,
    .unlocked_ioctl = basicbtfs_ioctl,
};
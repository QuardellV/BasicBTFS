#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"
#include "btree.h"
#include "btreecache.h"
#include "nametree.h"
#include "cache.h"

static int basicbtfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    uint32_t name_bno = 0;
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;

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

    printk(KERN_INFO "START Debug btree iterate\n");
    basicbtfs_btree_traverse_debug(sb, inode_info->i_bno);
    printk(KERN_INFO "END Debug btree iterate\n");

    if (basicbtfs_cache_iterate_dir(sb, inode_info->i_bno, ctx, ctx->pos - 2)) {
        printk("found in cache\n");
        return 0;
    }

    bh = sb_bread(sb, inode_info->i_bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;
    name_bno = node->tree_name_bno;
    brelse(bh);
    // basicbtfs_nametree_iterate_name_debug(sb, name_bno);
    // return basicbtfs_btree_traverse(sb, inode_info->i_bno, ctx, ctx->pos - 2, &ctx_index);
    return basicbtfs_nametree_iterate_name(sb, name_bno, ctx, ctx->pos - 2);
}

struct dentry *basicbtfs_search_entry(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    struct inode *inode = NULL;
    uint32_t ino = 0, hash = 0;

    hash = get_hash(dentry);

    // ino = basicbtfs_cache_lookup_entry(sb, inode_info->i_bno, hash);

    // if (ino != 0 && ino != -1) {
    //     inode = basicbtfs_iget(sb, ino);
    //     goto end;
    // }

    ino = basicbtfs_btree_node_lookup(sb, inode_info->i_bno, hash, 0);

    if (ino != 0 && ino != -1) {
        inode = basicbtfs_iget(sb, ino);
    }

    // end:
    dir->i_atime = current_time(dir);
    d_add(dentry, inode);
    return NULL;
}

int basicbtfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry) {
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    int ret = 0;
    struct basicbtfs_entry new_entry;
    uint32_t name_bno = 0, hash = 0, dir_bno = 0;
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_btree_node_cache *node_cache = NULL;

    bh = sb_bread(dir->i_sb, inode_info->i_bno);
    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;
    name_bno = node->tree_name_bno;
    brelse(bh);

    hash = get_hash(dentry);

    ret = basicbtfs_btree_node_lookup(dir->i_sb, inode_info->i_bno, hash, 0);

    if (ret != -1 && ret > 0) {
        printk(KERN_INFO "Filename %s already exists\n", dentry->d_name.name);
        return -EIO;
    }

    new_entry.ino = inode->i_ino;
    new_entry.hash = hash;

    printk(KERN_INFO "START Debug btree traverse added before: %s\n", dentry->d_name.name);
    basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    printk(KERN_INFO "CACHE: current dir_bno: %d\n", inode_info->i_bno);
    basicbtfs_cache_iterate_dir_debug(dir->i_sb, inode_info->i_bno);
    printk(KERN_INFO "END Debug btree traverse before\n");


    ret = basicbtfs_nametree_insert_name(dir->i_sb, name_bno, &new_entry, dentry, inode_info->i_bno);
    printk("added to nametree succesfully before addition: %d\n", inode_info->i_bno);


    ret = basicbtfs_btree_node_insert(dir->i_sb, dir, inode_info->i_bno, &new_entry);
    printk("added to btree succesfully: %d\n", inode_info->i_bno);

    node_cache = basicbtfs_cache_get_root_node(inode_info->i_bno);
    ret = basicbtfs_btree_node_cache_insert(dir->i_sb, dir, node_cache, &new_entry, inode_info->i_bno);

    
    printk(KERN_INFO "START Debug btree traverse added: %s\n", dentry->d_name.name);
    basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    printk(KERN_INFO "CACHE: current dir_bno: %d\n", inode_info->i_bno);
    basicbtfs_cache_iterate_dir_debug(dir->i_sb, inode_info->i_bno);
    printk(KERN_INFO "END Debug btree traverse\n");
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
    struct basicbtfs_entry new_entry;

    bh = sb_bread(dir->i_sb, inode_info->i_bno);
    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;
    name_bno = node->tree_name_bno;
    brelse(bh);

    printk(KERN_INFO "START Debug tree traverse BEFORE REMOVE: %s\n", dentry->d_name.name);
    // basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    // // // basicbtfs_btree_traverse_debug(dir->i_sb, inode_info->i_bno);
    // printk(KERN_INFO "END Debu tree traverse REMOVE\n");

    hash = get_hash(dentry);

    ino = basicbtfs_btree_node_lookup_with_entry(dir->i_sb, inode_info->i_bno, hash, 0, &new_entry);

    if (ino != 0 && ino != -1) {
        printk("yes it exists: %d | %d\n", new_entry.name_bno, new_entry.block_index);
    }

    ino = basicbtfs_btree_node_cache_lookup(basicbtfs_cache_get_root_node(inode_info->i_bno), hash, 0);

    if (ino != 0 && ino != -1) {
        printk("yes it exists in cache: %d | %d\n", new_entry.name_bno, new_entry.block_index);
    }

    printk(KERN_INFO "START Debug btree traverse BEFOREremove: %s\n", dentry->d_name.name);
    basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    printk(KERN_INFO "CACHE\n");
    basicbtfs_cache_iterate_dir_debug(dir->i_sb, inode_info->i_bno);
    printk(KERN_INFO "END Debug btree traverse BEFORE\n");

    ret = basicbtfs_btree_delete_entry(dir->i_sb, dir, inode_info->i_bno, hash);
    node_cache = basicbtfs_cache_get_root_node(inode_info->i_bno);
    ret = basicbtfs_btree_cache_delete_entry(dir->i_sb, dir, node_cache, hash, inode_info->i_bno);

    ret = basicbtfs_nametree_delete_name(dir->i_sb, new_entry.name_bno, new_entry.block_index, inode_info->i_bno);

    printk(KERN_INFO "START Debug btree traverse AFTER : %s\n", dentry->d_name.name);
    basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    printk(KERN_INFO "CACHE\n");
    basicbtfs_cache_iterate_dir_debug(dir->i_sb, inode_info->i_bno);
    printk(KERN_INFO "END Debug btree traverse AFTER\n");
    // basicbtfs_nametree_iterate_name_debug(dir->i_sb, name_bno);
    // // basicbtfs_btree_traverse_debug(dir->i_sb, inode_info->i_bno);
    // printk(KERN_INFO "END Debu tree traverse REMOVE\n");

    return ret;
}

int basicbtfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags) {
    struct super_block *sb = old_dir->i_sb;
    struct inode *old_inode = d_inode(old_dentry);
    struct inode *new_inode = d_inode(new_dentry);
    struct basicbtfs_inode_info *new_dir_info = BASICBTFS_INODE(new_dir);
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node * node = NULL;
    uint32_t hash = 0;
    int ret = 0;
    printk("hola1\n");

    if (flags & (RENAME_WHITEOUT | RENAME_NOREPLACE)) {
        return -EINVAL;
    }

    if (flags & (RENAME_EXCHANGE)) {
        if (!(old_inode && new_inode)) {
            return -EINVAL;
        }
    }

    hash = get_hash(new_dentry);

    ret = basicbtfs_btree_node_lookup(sb, new_dir_info->i_bno, hash, 0);

    if (ret != -1 && ret > 0) {
        printk("duplicate name\n");
        return -EEXIST;
        // if (flags & (RENAME_NOREPLACE) && new_dir == old_dir) {
        //     return -EEXIST;
        // } else {
        //     // if (new_inode) {
        //     ret = basicbtfs_delete_entry(old_dir, (char *)old_dentry->d_name.name);
        //     // }
        //     if (ret < 0) return -EIO;

        //     // ret = basicbtfs_btree_node_insert(sb, new_dir, new_dir_info->i_bno, (char *)new_dentry->d_name.name, old_inode->i_ino);
        //     ret = basicbtfs_btree_node_update(sb, new_dir_info->i_bno, (char *)new_dentry->d_name.name, 0, old_inode->i_ino);
        //     // ret = basicbtfs_add_entry(new_dir, old_inode, new_dentry);

        //     // if (new_dir != old_dir && strncmp) {
        //     // ret = basicbtfs_delete_entry(old_dir, (char *)old_dentry->d_name.name);
        //     // }

        //     return ret;
        // }
    }

    bh = sb_bread(sb, new_dir_info->i_bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    if (node->nr_of_files >= BASICBTFS_ENTRIES_PER_DIR) {
        brelse(bh);
        return -EMLINK;
    }

    brelse(bh);

    printk("hol2b\n");

    ret = basicbtfs_add_entry(new_dir, old_inode, new_dentry);
    printk("hol3a\n");

    if (ret < 0) return ret;

    ret = basicbtfs_delete_entry(old_dir, old_dentry);
    printk("hola4\n");
    return ret;
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

    while (bi < BASICBTFS_ATABLE_MAX_BLOCKS && file_block->table[bi] != 0) {
        char *block;

        put_blocks(sbi, file_block->table[bi], 1);
        // dir->i_blocks -= file_block->clusters[ci].nr_of_blocks;

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
    .iterate_shared = basicbtfs_iterate,
};
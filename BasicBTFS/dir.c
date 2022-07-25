#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"
#include "btree.h"

static int basicbtfs_iterate(struct file *dir, struct dir_context *ctx) {
    struct inode *inode = file_inode(dir);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    loff_t ctx_index = 0;

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

    // printk(KERN_INFO "START Debug btree iterate\n");
    // basicbtfs_btree_traverse_debug(sb, inode_info->i_bno);
    // printk(KERN_INFO "END Debug btree iterate\n");

    return basicbtfs_btree_traverse(sb, inode_info->i_bno, ctx, ctx->pos - 2, &ctx_index);
}

struct dentry *basicbtfs_search_entry(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    struct inode *inode = NULL;
    uint32_t ino = 0;

    ino = basicbtfs_btree_node_lookup(sb, inode_info->i_bno, (char *)dentry->d_name.name, 0);

    if (ino != 0 && ino != -1) {
        inode = basicbtfs_iget(sb, ino);
    }

    dir->i_atime = current_time(dir);
    d_add(dentry, inode);
    return NULL;
}

int basicbtfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry) {
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    int ret = 0;

    ret = basicbtfs_btree_node_insert(dir->i_sb, dir, inode_info->i_bno, (char *)dentry->d_name.name, inode->i_ino);

    // printk(KERN_INFO "START Debug btree traverse added: %s\n", dentry->d_name.name);
    // basicbtfs_btree_traverse_debug(dir->i_sb, inode_info->i_bno);
    // printk(KERN_INFO "END Debug btree traverse\n");

    return ret;
}

int basicbtfs_delete_entry(struct inode *dir, char *filename) {
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(dir);
    int ret = 0;

    ret = basicbtfs_btree_delete_entry(dir->i_sb, dir, inode_info->i_bno, filename);

    printk(KERN_INFO "START Debug tree traverse REMOVE: %s\n", filename);
    basicbtfs_btree_traverse_debug(dir->i_sb, inode_info->i_bno);
    printk(KERN_INFO "END Debu tree traverse REMOVE\n");

    return ret;
}

int basicbtfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags) {
    struct super_block *sb = old_dir->i_sb;
    struct inode *old_inode = d_inode(old_dentry);
    struct inode *new_inode = d_inode(new_dentry);
    struct basicbtfs_inode_info *new_dir_info = BASICBTFS_INODE(new_dir);
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node * node = NULL;
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

    ret = basicbtfs_btree_node_lookup(sb, new_dir_info->i_bno, (char *)new_dentry->d_name.name, 0);
    printk("hola2\n");

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

    ret = basicbtfs_delete_entry(old_dir, (char *)old_dentry->d_name.name);
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
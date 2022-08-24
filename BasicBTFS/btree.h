#ifndef BASICBTFS_BTREE_H
#define BASICBTFS_BTREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "bitmap.h"
#include "cache.h"

static inline int basicbtfs_btree_node_delete(struct super_block *sb, uint32_t bno, uint32_t hash);

static inline void basicbtfs_btree_node_init(struct super_block *sb, struct basicbtfs_btree_node *node, bool leaf, uint32_t dir_bno, uint32_t bno, uint32_t par_bno) {
    // printk("new btree node: %d\n", bno);
    memset(node, 0, sizeof(struct basicbtfs_btree_node));
    node->nr_of_keys = 0;
    node->leaf = leaf;
    node->parent = par_bno;
    // node->block_type = BASICBTFS_BLOCKTYPE_BTREE_NODE;
    // basicbtfs_cache_add_node(sb, dir_bno, bno, node);
}

static inline int basicbtfs_btree_update_root(struct inode *inode, uint32_t bno) {
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct basicbtfs_inode *disk_inode = NULL;
    struct buffer_head *bh = NULL;
    
    uint32_t ino = inode->i_ino;
    uint32_t inode_block = BASICBTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_offset = BASICBTFS_GET_INODE_BLOCK_IDX(ino);

    if (ino >= sbi->s_ninodes) return -1;

    bh = sb_bread(sb, inode_block);

    if (!bh) return -EIO;

    // printk("update root: %d | %d\n", inode_info->i_bno, bno);

    // printk(KERN_INFO "START Debug btree traverse added update before\n");
    // printk(KERN_INFO "CACHE: current dir_bno: %d\n", inode_info->i_bno);
    // basicbtfs_cache_iterate_dir_debug(inode->i_sb, inode_info->i_bno);
    // printk(KERN_INFO "END Debug btree traverse update before\n");

    // basicbtfs_cache_update_root_node(sb, inode_info->i_bno, bno);
    basicbtfs_cache_update_root_bno(inode_info->i_bno, bno);

    // printk(KERN_INFO "START Debug btree traverse added update after\n");
    // printk(KERN_INFO "CACHE: current dir_bno: %d\n", bno);
    // basicbtfs_cache_iterate_dir_debug(inode->i_sb, bno);
    // printk(KERN_INFO "END Debug btree traverse update after\n");

    disk_inode = (struct basicbtfs_inode *) bh->b_data;
    disk_inode += inode_offset;

    inode_info->i_bno = bno;
    disk_inode->i_bno = bno;

    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}

static inline uint32_t basicbtfs_btree_node_lookup(struct super_block *sb, uint32_t root_bno, uint32_t hash, int counter) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *btr_node = NULL;
    uint32_t ret = 0, child = 0;
    int index = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return 0;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    btr_node = &disk_block->block_type.btree_node;
    // hash > btr_node->entries[index].hash
    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }
    // btr_node->entries[index].hash == hash
    if (btr_node->entries[index].hash == hash) {
        // printk(KERN_INFO "Current counter: %d of %d\n", counter, hash);
        ret = btr_node->entries[index].ino;
        brelse(bh);
        return ret;
    }

    if (btr_node->leaf) {
        brelse(bh);
        return 0;
    }
    child  = btr_node->children[index];
    brelse(bh);
    ret = basicbtfs_btree_node_lookup(sb, child, hash, counter);
    return ret;
}

static inline int basicbtfs_btree_free_dir(struct super_block *sb, struct inode *inode, uint32_t bno) {
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
        printk("basicbtfs_defrag_btree: bno: %d\n", bno);
        return -1;
    }

    printk("new bno for btree: %d\n", bno);
    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
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

        // printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_free_dir(sb, inode, node->children[index]);
        if (ret == -1) return ret;
    }

    memset(disk_block, 0, sizeof(struct basicbtfs_disk_block));
    put_blocks(sbi, bno, 1);
    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}

static inline uint32_t basicbtfs_btree_node_update_namelist_info(struct super_block *sb, uint32_t root_bno, uint32_t hash, int counter, uint32_t name_bno, uint32_t block_index) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *btr_node = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    uint32_t ret = 0, child = 0;
    int index = 0;

    if (root_bno == 0 || root_bno > sbi->s_nblocks) {
        printk("something went wrong: root bno %d\n", root_bno);
        return -1;
    }

    bh = sb_bread(sb, root_bno);

    if (!bh) return 0;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    btr_node = &disk_block->block_type.btree_node;

    // hash > btr_node->entries[index].hash
    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }

    // btr_node->entries[index].hash == hash
    if (btr_node->entries[index].hash == hash) {
        printk("found :)\n");
        btr_node->entries[index].name_bno = name_bno;
        btr_node->entries[index].block_index = block_index;
        mark_buffer_dirty(bh);
        brelse(bh);
        return 0;
    }

    if (btr_node->leaf) {
        brelse(bh);
        printk("couldn't find hash\n");
        return -1;
    }
    child = btr_node->children[index];
    brelse(bh);
    ret = basicbtfs_btree_node_update_namelist_info(sb, child, hash, counter, name_bno, block_index);
    return ret;
}

static inline uint32_t basicbtfs_btree_node_lookup_with_entry(struct super_block *sb, uint32_t root_bno, uint32_t hash, int counter, struct basicbtfs_entry *entry) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *btr_node = NULL;
    uint32_t ret = 0, child = 0;
    int index = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return 0;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    btr_node = &disk_block->block_type.btree_node;

    // hash > btr_node->entries[index].hash
    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }
    // btr_node->entries[index].hash == hash
    if (btr_node->entries[index].hash == hash) {
        // printk(KERN_INFO "Current counter: %d of %d with: %d | %d\n", counter, hash, btr_node->entries[index].name_bno, btr_node->entries[index].block_index);
        ret = btr_node->entries[index].ino;
        memcpy(entry, &btr_node->entries[index], sizeof(struct basicbtfs_entry));
        brelse(bh);
        return ret;
    }

    if (btr_node->leaf) {
        brelse(bh);
        return 0;
    }
    child  = btr_node->children[index];
    brelse(bh);
    ret = basicbtfs_btree_node_lookup_with_entry(sb, child, hash, counter, entry);
    return ret;
}

static inline int basicbtfs_btree_split_child(struct super_block *sb, uint32_t par, uint32_t lhs, int index, uint32_t dir_bno) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *node_par = NULL, *node_lhs = NULL, *node_rhs = NULL;
    uint32_t rhs = get_free_blocks(sbi, 1);
    int i = 0;

    bh_par = sb_bread(sb, par);

    if (!bh_par) return -EIO;

    // node_par = (struct basicbtfs_btree_node *) bh_par->b_data;

    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node_par = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, lhs);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    node_lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, rhs);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    disk_block->block_type_id = BASICBTFS_BLOCKTYPE_BTREE_NODE;
    node_rhs = &disk_block->block_type.btree_node;

    basicbtfs_btree_node_init(sb, node_rhs, node_lhs->leaf, dir_bno, rhs, par);
    node_rhs->root = false;
    node_rhs->nr_of_keys = BASICBTFS_MIN_DEGREE - 1;

    for (i = 0; i < node_rhs->nr_of_keys; i++) {
        memcpy(&node_rhs->entries[i], &node_lhs->entries[i + BASICBTFS_MIN_DEGREE], sizeof(struct basicbtfs_entry));
    }

    if (!node_lhs->leaf) {
        for (i = 0; i < BASICBTFS_MIN_DEGREE; i++) {
            node_rhs->children[i] = node_lhs->children[i + BASICBTFS_MIN_DEGREE];
        }
    }

    node_lhs->nr_of_keys = BASICBTFS_MIN_DEGREE - 1;

    for (i = node_par->nr_of_keys; i >= index + 1; i--) {
        node_par->children[i+1] = node_par->children[i];
    }

    node_par->children[index+1] = rhs;

    for (i = node_par->nr_of_keys - 1; i >= index; i--) {
        memcpy(&node_par->entries[i + 1], &node_par->entries[i], sizeof(struct basicbtfs_entry));
    }

    memcpy(&node_par->entries[index], &node_lhs->entries[BASICBTFS_MIN_DEGREE - 1], sizeof(struct basicbtfs_entry));
    node_par->nr_of_keys++;

    mark_buffer_dirty(bh_par);
    mark_buffer_dirty(bh_lhs);
    mark_buffer_dirty(bh_rhs);
    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);
    return 0;
}

static inline int basicbtfs_btree_insert_non_full(struct super_block *sb, uint32_t bno, struct basicbtfs_entry *new_entry, uint32_t dir_bno) {
    struct buffer_head *bh = NULL, *bh_child = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *node = NULL, *child = NULL;
    int ret = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    if (node->leaf) {
        int index = node->nr_of_keys - 1;
        // node->entries[index].hash > new_entry->hash
        while (index >= 0 && node->entries[index].hash > new_entry->hash) {
            memcpy(&node->entries[index + 1], &node->entries[index], sizeof(struct basicbtfs_entry));
            index--;
        }

        memcpy(&node->entries[index + 1], new_entry, sizeof(struct basicbtfs_entry));
        node->nr_of_keys++;
        mark_buffer_dirty(bh);
    } else {
        int index = node->nr_of_keys - 1;
        // node->entries[index].hash > new_entry->hash
        while (index >= 0 && node->entries[index].hash > new_entry->hash) {
            index--;
        }

        bh_child = sb_bread(sb, node->children[index + 1]);

        if (!bh_child) {
            brelse(bh);
            return -EIO;
        }

        disk_block = (struct basicbtfs_disk_block *) bh_child->b_data;
        child = &disk_block->block_type.btree_node;

        if (child->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE - 1) {
            ret = basicbtfs_btree_split_child(sb, bno, node->children[index + 1], index + 1, dir_bno);

            if (ret != 0) {
                brelse(bh);
                brelse(bh_child);
                return ret;
            }
            // node->entries[index+1].hash < new_entry->hash
            if (node->entries[index+1].hash < new_entry->hash) {
                index++;
            }
        }
        brelse(bh_child);
        basicbtfs_btree_insert_non_full(sb, node->children[index + 1], new_entry, dir_bno);

    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_update(struct super_block *sb, uint32_t root_bno, uint32_t hash, int counter, uint32_t inode) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *btr_node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t ret = 0;
    int index = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return 0;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    btr_node = &disk_block->block_type.btree_node;
    // hash > btr_node->entries[index].hash
    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }
    // btr_node->entries[index].hash == hash
    if (btr_node->entries[index].hash == hash) {
        // printk(KERN_INFO "Current counter: %d\n", counter);
        btr_node->entries[index].ino = inode;
        mark_buffer_dirty(bh);
        brelse(bh);
        return ret;
    }

    if (btr_node->leaf) {
        brelse(bh);
        return 0;
    }
    ret =  basicbtfs_btree_node_update(sb, btr_node->children[index], hash, counter, inode);
    brelse(bh);
    return ret;
}

static inline int basicbtfs_btree_node_insert(struct super_block *sb, struct inode *par_inode, uint32_t bno, struct basicbtfs_entry *entry) {
    struct buffer_head *bh_old = NULL, *bh_new = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node * old_node = NULL, *new_node = NULL;
    int ret = 0;

    bh_old = sb_bread(sb, bno);

    if (!bh_old) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh_old->b_data;
    old_node = &disk_block->block_type.btree_node;
    // old_node = (struct basicbtfs_btree_node *) bh_old->b_data;

    // printk("nr of keys non cache: %d\n", old_node->nr_of_keys);

    if (old_node->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE -1) {
        int index = 0;
        uint32_t bno_new_root = get_free_blocks(sbi, 1);

        bh_new = sb_bread(sb, bno_new_root);

        if (!bh_new) {
            brelse(bh_old);
            return -EIO;
        }

        // new_node = (struct basicbtfs_btree_node *) bh_new->b_data;
        disk_block = (struct basicbtfs_disk_block *) bh_new->b_data;
        disk_block->block_type_id = BASICBTFS_BLOCKTYPE_BTREE_NODE;
        new_node = &disk_block->block_type.btree_node;
        basicbtfs_btree_node_init(sb, new_node, false, bno, bno_new_root, par_inode->i_ino);
        new_node->children[0] = bno;

        ret = basicbtfs_btree_split_child(sb, bno_new_root, bno, 0, bno);

        if (ret != 0) {
            brelse(bh_old);
            brelse(bh_new);
            return ret;
        }
        // new_node->entries[index].hash < entry->hash
        if (new_node->entries[index].hash < entry->hash) {
            index++;
        }

        ret = basicbtfs_btree_insert_non_full(sb, new_node->children[index], entry, bno);

        if (ret != 0) {
            brelse(bh_old);
            brelse(bh_new);
            return ret;
        }

        ret = basicbtfs_btree_update_root(par_inode, bno_new_root);

        if (ret != 0) {
            brelse(bh_old);
            brelse(bh_new);
            return ret;
        }

        new_node->nr_of_files = old_node->nr_of_files + 1;
        new_node->nr_times_done = old_node->nr_times_done + 1;
        new_node->tree_name_bno = old_node->tree_name_bno;
        new_node->parent = par_inode->i_ino;
        new_node->root = true;
        old_node->root = false;
        old_node->parent = bno_new_root;
        mark_buffer_dirty(bh_new);
        brelse(bh_new);
    } else {
        ret = basicbtfs_btree_insert_non_full(sb, bno, entry, bno);

        if (ret != 0) {
            brelse(bh_old);
            return ret;
        }
        old_node->nr_of_files++;
        old_node->nr_times_done++;
        mark_buffer_dirty(bh_old);
    }
    brelse(bh_old);
    return 0;
}

static inline int basicbtfs_btree_node_find_key(struct super_block *sb, uint32_t bno, uint32_t hash) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int index = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -1;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    // node = (struct basicbtfs_btree_node *) bh->b_data;
    // node->entries[index].hash < hash
    while (index < node->nr_of_keys && node->entries[index].hash < hash) {
        ++index;
    }
    brelse(bh);
    // printk(KERN_INFO "Founded key: %d for filename %d\n", index, hash);
    return index;
}

static inline int basicbtfs_btree_node_remove_from_leaf(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int i = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -1;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    // printk(KERN_INFO "nr of keys: %d\n", index);

    for (i = index + 1; i < node->nr_of_keys; i++) {
        memcpy(&node->entries[i - 1], &node->entries[i], sizeof(struct basicbtfs_entry));
    }

    node->nr_of_keys--;
    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_get_predecessor(struct super_block *sb, uint32_t bno, int index, struct basicbtfs_entry *ret) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t bno_child = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    bno_child = node->children[index];

    brelse(bh);
    bh = NULL;
    node = NULL;
    bh = sb_bread(sb, bno_child);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;


    while (!node->leaf) {
        uint32_t bno_child = node->children[node->nr_of_keys];
        brelse(bh);
        bh = NULL;
        node = NULL;

        bh = sb_bread(sb, bno_child);

        if (!bh) return -EIO;
        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        node = &disk_block->block_type.btree_node;

        // node = (struct basicbtfs_btree_node *) bh->b_data;
    }

    memcpy(ret, &node->entries[node->nr_of_keys- 1], sizeof (struct basicbtfs_entry));
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_get_succesor(struct super_block *sb, uint32_t bno, int index, struct basicbtfs_entry *ret) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    uint32_t bno_child = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    bno_child = node->children[index + 1];

    brelse(bh);
    bh = NULL;
    node = NULL;
    bh = sb_bread(sb, bno_child);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    while (!node->leaf) {
        uint32_t bno_child = node->children[0];
        brelse(bh);
        bh = NULL;
        node = NULL;

        bh = sb_bread(sb, bno_child);

        if (!bh) return -EIO;

        // node = (struct basicbtfs_btree_node *) bh->b_data;
        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        node = &disk_block->block_type.btree_node;
    }

    memcpy(ret, &node->entries[0], sizeof (struct basicbtfs_entry));
    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_merge(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh_par->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    // lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    // rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    rhs = &disk_block->block_type.btree_node;


    memcpy(&lhs->entries[BASICBTFS_MIN_DEGREE - 1], &node->entries[index], sizeof(struct basicbtfs_entry));

    for (i  = 0; i < rhs->nr_of_keys; i++) {
        memcpy(&lhs->entries[i + BASICBTFS_MIN_DEGREE], &rhs->entries[i], sizeof(struct basicbtfs_entry));
    }

    if (!lhs->leaf) {
        for (i = 0; rhs->nr_of_keys; i++) {
            lhs->children[i + BASICBTFS_MIN_DEGREE] = rhs->children[i];
        }
    }

    for (i = index + 1; i < node->nr_of_keys; i++) {
        memcpy(&node->entries[i - 1], &node->entries[i], sizeof(struct basicbtfs_entry));
    }

    for (i = index + 2; i <= node->nr_of_keys; i++) {
        node->children[i - 1] = node->children[i];
    }

    lhs->nr_of_keys = lhs->nr_of_keys + rhs->nr_of_keys + 1;
    node->nr_of_keys--;
    put_blocks(BASICBTFS_SB(sb), node->children[index + 1], 1);
    node->children[index + 1] = 0;

    mark_buffer_dirty(bh_par);
    mark_buffer_dirty(bh_lhs);
    mark_buffer_dirty(bh_rhs);

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);
    return 0;
}

static inline int basicbtfs_btree_node_remove_from_nonleaf(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_entry tmp, pred, succ;
    int ret = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh_par->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    // rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    rhs = &disk_block->block_type.btree_node;

    if (lhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_get_predecessor(sb, bno, index, &pred);

        if (ret != 0) {
            brelse(bh_par);
            brelse(bh_lhs);
            brelse(bh_rhs);
            return ret;
        }
        memcpy(&node->entries[index], &pred, sizeof(struct basicbtfs_entry));
        mark_buffer_dirty(bh_par);
        ret = basicbtfs_btree_node_delete(sb, node->children[index], pred.hash);
    } else if (rhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_get_succesor(sb, bno, index, &succ);

        if (ret != 0) {
            brelse(bh_par);
            brelse(bh_lhs);
            brelse(bh_rhs);
            return ret;
        }

        memcpy(&node->entries[index], &succ, sizeof(struct basicbtfs_entry));
        mark_buffer_dirty(bh_par);
        ret = basicbtfs_btree_node_delete(sb, node->children[index + 1], succ.hash);
    } else {
        memcpy(&tmp, &node->entries[index], sizeof(struct basicbtfs_entry));
        ret = basicbtfs_btree_node_merge(sb, bno, index);

        if (ret != 0) {
            brelse(bh_par);
            brelse(bh_lhs);
            brelse(bh_rhs);
            return ret;
        }

        ret = basicbtfs_btree_node_delete(sb, node->children[index], tmp.hash);
    }

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);

    return 0;
}

static inline int basicbtfs_btree_node_steal_from_previous(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh_par->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    // lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    // rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    rhs = &disk_block->block_type.btree_node;

    for (i = rhs->nr_of_keys - 1; i >= 0; --i) {
        memcpy(&rhs->entries[i+1], &rhs->entries[i], sizeof(struct basicbtfs_entry));
    }

    if (!rhs->leaf) {
        for (i = rhs->nr_of_keys; i >= 0; --i) {
            rhs->children[i+1] = rhs->children[i];
        }
    }

    memcpy(&rhs->entries[0], &node->entries[index - 1], sizeof(struct basicbtfs_entry));

    if (!rhs->leaf) {
        rhs->children[0] = lhs->children[lhs->nr_of_keys];
    }

    memcpy(&node->entries[index - 1], &lhs->entries[lhs->nr_of_keys - 1], sizeof(struct basicbtfs_entry));

    rhs->nr_of_keys++;
    lhs->nr_of_keys--;

    mark_buffer_dirty(bh_par);
    mark_buffer_dirty(bh_lhs);
    mark_buffer_dirty(bh_rhs);

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);
    return 0;
}

static inline int basicbtfs_btree_node_steal_from_next(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh_par->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    // lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    // rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    rhs = &disk_block->block_type.btree_node;

    memcpy(&lhs->entries[lhs->nr_of_keys], &node->entries[index], sizeof(struct basicbtfs_entry));

    if (!lhs->leaf) {
        lhs->children[lhs->nr_of_keys+1] = rhs->children[0];
    }

    memcpy(&node->entries[index], &rhs->entries[0], sizeof(struct basicbtfs_entry));

    for (i = 1; i < rhs->nr_of_keys; i++) {
        memcpy(&rhs->entries[i-1], &rhs->entries[i], sizeof(struct basicbtfs_entry));
    }

    if (!rhs->leaf) {
        for (i = 1; i <= rhs->nr_of_keys; i++) {
            rhs->children[i-1] = rhs->children[i];
        }
    }

    lhs->nr_of_keys++;
    rhs->nr_of_keys--;

    mark_buffer_dirty(bh_par);
    mark_buffer_dirty(bh_lhs);
    mark_buffer_dirty(bh_rhs);

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);
    return 0;
}

static inline int basicbtfs_btree_node_fill(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int ret = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh_par->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_par->b_data;
    node = &disk_block->block_type.btree_node;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    // lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_lhs->b_data;
    lhs = &disk_block->block_type.btree_node;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    // rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh_rhs->b_data;
    rhs = &disk_block->block_type.btree_node;

    if (index != 0 && lhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_steal_from_previous(sb, bno, index);
    } else if (index != node->nr_of_keys && rhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_steal_from_next(sb, bno, index);
    } else {
        if (index != node->nr_of_keys) {
            ret = basicbtfs_btree_node_merge(sb, bno, index);
        } else {
            ret = basicbtfs_btree_node_merge(sb, bno, index - 1);
        }
    }

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);
    return 0;
}

static inline int basicbtfs_btree_node_delete(struct super_block *sb, uint32_t bno, uint32_t hash) {
    struct buffer_head *bh = NULL, *bh2 = NULL;
    struct basicbtfs_btree_node *node = NULL, *child = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int index = basicbtfs_btree_node_find_key(sb, bno, hash);
    int ret = 0;
    bool flag = false;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    // node->entries[index].hash == new_entry->hash
    if (index < node->nr_of_keys && node->entries[index].hash == hash) {
        if (node->leaf) {
            // printk("leaf\n");
            ret = basicbtfs_btree_node_remove_from_leaf(sb, bno, index);
        } else {
            // printk("no leaf\n");
            ret = basicbtfs_btree_node_remove_from_nonleaf(sb, bno, index);
        }
    } else {
        if (node->leaf) {
            printk(KERN_INFO "File %d doesn't exist with index %d\n", hash, index);
        }

        bh2 = sb_bread(sb, node->children[index]);

        if (!bh2) {
            brelse(bh);
            return -EIO;
        }

        // child = (struct basicbtfs_btree_node *) bh2->b_data;
        disk_block = (struct basicbtfs_disk_block *) bh2->b_data;
        child = &disk_block->block_type.btree_node;

        flag = (index == node->nr_of_keys) ? true : false;

        if (child->nr_of_keys < BASICBTFS_MIN_DEGREE) {
            ret = basicbtfs_btree_node_fill(sb, bno, index);

            if (ret != 0) {
                brelse(bh);
                brelse(bh2);
                return ret;
            }
        }

        basicbtfs_btree_node_delete(sb, node->children[index], hash);
        brelse(bh2);
    }
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_delete_entry(struct super_block *sb, struct inode *inode, uint32_t root_bno, uint32_t hash) {
    struct buffer_head *bh = NULL, *bh2 = NULL;
    struct basicbtfs_btree_node *node, *new_root_node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int ret = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    ret = basicbtfs_btree_node_delete(sb, root_bno, hash);

    if (ret == 1) {
        brelse(bh);
        return 0;
    } else if (ret < 0) {
        brelse(bh);
        return ret;
    }

    if (node->nr_of_keys == 0) {
        if (node->leaf) {
            node->nr_of_files--;
            mark_buffer_dirty(bh);
        } else {
            ret = basicbtfs_btree_update_root(inode, node->children[0]);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }

            bh2 = sb_bread(sb, node->children[0]);

            if (!bh2) {
                brelse(bh);
                return -EIO;
            }

            // new_root_node = (struct basicbtfs_btree_node *) bh2->b_data;
            disk_block = (struct basicbtfs_disk_block *) bh2->b_data;
            new_root_node = &disk_block->block_type.btree_node;
            new_root_node->nr_of_files = node->nr_of_files - 1;
            new_root_node->nr_times_done = node->nr_times_done;
            new_root_node->tree_name_bno = node->tree_name_bno;
            new_root_node->parent = inode->i_ino;
            new_root_node->root = true;
            node->root = false;
            // basicbtfs_cache_delete_node(sb, node->children[0], root_bno);
            mark_buffer_dirty(bh2);
            brelse(bh2);
        }
    } else {
        node->nr_of_files--;
        mark_buffer_dirty(bh);
    }

    brelse(bh);
    return ret;
}

static inline int basicbtfs_btree_clear(struct super_block *sb, uint32_t bno) {
    return 0;
}

static inline int basicbtfs_btree_traverse_debug(struct super_block *sb, uint32_t bno) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int index = 0, ret = 0;
    bh = sb_bread(sb, bno);


    if (!bh) return -EIO;


    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    if (node->root) {
        printk("nr of files and keys: %d | %d\n", node->nr_of_files, node->nr_of_keys);
    }

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_btree_traverse_debug(sb, node->children[index]);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }
        printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_traverse_debug(sb, node->children[index]);
    }

    brelse(bh);
    return 0;
}

#endif

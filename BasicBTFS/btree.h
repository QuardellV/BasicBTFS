#ifndef BASICBTFS_BTREE_H
#define BASICBTFS_BTREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "bitmap.h"

static inline void basicbtfs_btree_node_init(struct basicbtfs_btree_node *node, bool leaf) {

}

static inline uint32_t basicbtfs_btree_node_lookup(struct super_block *sb, uint32_t root_bno, char *filename, int counter) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *btr_node = NULL;
    uint32_t ret = 0, child = 0;
    int index = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return 0;

    btr_node = (struct basicbtfs_btree_node *) bh->b_data;

    while (index < btr_node->nr_of_keys && strncmp(filename, btr_node->entries[index].hash_name, BASICBTFS_NAME_LENGTH) > 0) {
        index++;
        counter++;
    }

    if (strncmp(btr_node->entries[index].hash_name, filename, BASICBTFS_NAME_LENGTH) == 0) {
        printk(KERN_INFO "Current counter: %d\n", counter);
        ret = btr_node->entries[index].ino;
        brelse(bh);
        return ret;
    }

    if (btr_node->leaf) {
        brelse(bh);
        return 0;
    }
    child = btr_node->children[index];
    brelse(bh);
    return basicbtfs_btree_node_lookup(sb, child, filename, counter);
}

static inline int basicbtfs_btree_node_insert(struct super_block *sb, struct inode *par_inode, uint32_t bno, char *filename, uint32_t inode) {
    return 0;
}

static inline int basicbtfs_btree_node_delete(struct super_block *sb, uint32_t bno, char *filename) {
    return 0;
}

static inline int basicbtfs_btree_delete_entry(struct super_block *sb, struct inode *inode, uint32_t root_bno, char *filename) {
    return 0;
}

static inline int basicbtfs_btree_clear(struct super_block *sb, uint32_t bno) {
    return 0;
}

static inline int basicbtfs_btree_traverse(struct super_block *sb, uint32_t bno, struct dir_context *ctx) {
    return 0;
}

static inline int basicbtfs_btree_traverse_debug(struct super_block *sb, uint32_t bno) {
    return 0;
}

#endif

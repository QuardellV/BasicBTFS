#ifndef BASICBTFS_BTREE_H
#define BASICBTFS_BTREE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "bitmap.h"

static inline int basicbtfs_btree_node_delete(struct super_block *sb, uint32_t bno, char *filename);

static inline void basicbtfs_btree_node_init(struct basicbtfs_btree_node *node, bool leaf) {
    memset(node, 0, sizeof(struct basicbtfs_btree_node));
    node->nr_of_keys = 0;
    node->leaf = leaf;
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

    disk_inode = (struct basicbtfs_inode *) bh->b_data;
    disk_inode += inode_offset;

    inode_info->i_bno = bno;
    disk_inode->i_bno = bno;
    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
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

static inline int basicbtfs_btree_split_child(struct super_block *sb, uint32_t par, uint32_t lhs, int index) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node_par = NULL, *node_lhs = NULL, *node_rhs = NULL;
    uint32_t rhs = get_free_blocks(sbi, 1);
    int i = 0;

    bh_par = sb_bread(sb, par);

    if (!bh_par) return -EIO;

    node_par = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, lhs);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    node_lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, rhs);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    node_rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

    basicbtfs_btree_node_init(node_rhs, node_lhs->leaf);
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

static inline int basicbtfs_btree_insert_non_full(struct super_block *sb, uint32_t bno, char *filename, uint32_t inode) {
    struct buffer_head *bh = NULL, *bh_child = NULL;
    struct basicbtfs_btree_node *node = NULL, *child = NULL;
    int ret = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    if (node->leaf) {
        int index = node->nr_of_keys - 1;

        while (index >= 0 && strncmp(node->entries[index].hash_name, filename, BASICBTFS_NAME_LENGTH) > 0) {
            memcpy(&node->entries[index + 1], &node->entries[index], sizeof(struct basicbtfs_entry));
            index--;
        }

        strncpy(node->entries[index + 1].hash_name, filename, BASICBTFS_NAME_LENGTH);
        node->entries[index + 1].ino = inode;
        node->nr_of_keys++;
        mark_buffer_dirty(bh);
    } else {
        int index = node->nr_of_keys - 1;

        while (index >= 0 && strncmp(node->entries[index].hash_name, filename, BASICBTFS_NAME_LENGTH) > 0) {
            index--;
        }

        bh_child = sb_bread(sb, node->children[index + 1]);

        if (!bh_child) {
            brelse(bh);
            return -EIO;
        }

        child = (struct basicbtfs_btree_node *) bh_child->b_data;

        if (child->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE - 1) {
            ret = basicbtfs_btree_split_child(sb, bno, node->children[index + 1], index + 1);

            if (ret != 0) {
                brelse(bh);
                brelse(bh_child);
                return ret;
            }

            if (strncmp(node->entries[index + 1].hash_name,filename , BASICBTFS_NAME_LENGTH) < 0) {
                index++;
            }
        }
        brelse(bh_child);
        basicbtfs_btree_insert_non_full(sb, node->children[index + 1], filename, inode);

    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_update(struct super_block *sb, uint32_t root_bno, char *filename, int counter, uint32_t inode) {
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
        btr_node->entries[index].ino = inode;
        mark_buffer_dirty(bh);
        brelse(bh);
        return ret;
    }

    if (btr_node->leaf) {
        brelse(bh);
        return 0;
    }
    child = btr_node->children[index];
    brelse(bh);
    return basicbtfs_btree_node_update(sb, child, filename, counter, inode);
}

static inline int basicbtfs_btree_node_insert(struct super_block *sb, struct inode *par_inode, uint32_t bno, char *filename, uint32_t inode) {
    struct buffer_head *bh_old = NULL, *bh_new = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_btree_node * old_node = NULL, *new_node = NULL;
    int ret = 0;

    bh_old = sb_bread(sb, bno);

    if (!bh_old) return -EIO;

    old_node = (struct basicbtfs_btree_node *) bh_old->b_data;

    ret = basicbtfs_btree_node_lookup(sb, bno, filename, 0);

    // This is necessary. Otherwise we would split unnecesary!
    if (ret != -1 && ret > 0) {
        printk(KERN_INFO "Filename %s already exists\n", filename);
        brelse(bh_old);
        return -EIO;
    }

    if (old_node->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE -1) {
        int index = 0;
        uint32_t bno_new_root = get_free_blocks(sbi, 1);

        bh_new = sb_bread(sb, bno_new_root);

        if (!bh_new) {
            brelse(bh_old);
            return -EIO;
        }

        new_node = (struct basicbtfs_btree_node *) bh_new->b_data;
        basicbtfs_btree_node_init(new_node, false);
        new_node->children[0] = bno;

        ret = basicbtfs_btree_split_child(sb, bno_new_root, bno, 0);

        if (ret != 0) {
            brelse(bh_old);
            brelse(bh_new);
            return ret;
        }

        if (strncmp(new_node->entries[0].hash_name, filename, BASICBTFS_NAME_LENGTH) < 0) {
            index++;
        }

        ret = basicbtfs_btree_insert_non_full(sb, new_node->children[index], filename, inode);

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
        mark_buffer_dirty(bh_new);
        brelse(bh_new);
    } else {
        ret = basicbtfs_btree_insert_non_full(sb, bno, filename, inode);

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

static inline int basicbtfs_btree_node_find_key(struct super_block *sb, uint32_t bno, char *filename) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int index = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -1;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    while (index < node->nr_of_keys && strncmp(node->entries[index].hash_name, filename, BASICBTFS_NAME_LENGTH) < 0) {
        ++index;
    }
    brelse(bh);
    printk(KERN_INFO "Founded key: %d\n", index);
    return index;
}

static inline int basicbtfs_btree_node_remove_from_leaf(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int i = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -1;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    for (i = index + 1; i < node->nr_of_keys; ++i) {
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
    uint32_t bno_child = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;
    bno_child = node->children[index];

    brelse(bh);
    bh = NULL;
    node = NULL;
    bh = sb_bread(sb, bno_child);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;


    while (!node->leaf) {
        uint32_t bno_child = node->children[node->nr_of_keys];
        brelse(bh);
        bh = NULL;
        node = NULL;

        bh = sb_bread(sb, bno_child);

        if (!bh) return -EIO;

        node = (struct basicbtfs_btree_node *) bh->b_data;
    }

    memcpy(ret, &node->entries[node->nr_of_keys- 1], sizeof (struct basicbtfs_entry));
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_get_succesor(struct super_block *sb, uint32_t bno, int index, struct basicbtfs_entry *ret) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    uint32_t bno_child = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;
    bno_child = node->children[index + 1];

    brelse(bh);
    bh = NULL;
    node = NULL;
    bh = sb_bread(sb, bno_child);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;


    while (!node->leaf) {
        uint32_t bno_child = node->children[0];
        brelse(bh);
        bh = NULL;
        node = NULL;

        bh = sb_bread(sb, bno_child);

        if (!bh) return -EIO;

        node = (struct basicbtfs_btree_node *) bh->b_data;
    }

    memcpy(ret, &node->entries[0], sizeof (struct basicbtfs_entry));
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_node_merge(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    node = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

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
    struct basicbtfs_entry tmp, pred, succ;
    int ret = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    node = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

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
        ret = basicbtfs_btree_node_delete(sb, node->children[index], pred.hash_name);
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
        ret = basicbtfs_btree_node_delete(sb, node->children[index + 1], succ.hash_name);
    } else {
        memcpy(&tmp, &node->entries[index], sizeof(struct basicbtfs_entry));
        ret = basicbtfs_btree_node_merge(sb, bno, index);

        if (ret != 0) {
            brelse(bh_par);
            brelse(bh_lhs);
            brelse(bh_rhs);
            return ret;
        }

        ret = basicbtfs_btree_node_delete(sb, node->children[index], tmp.hash_name);
    }

    brelse(bh_par);
    brelse(bh_lhs);
    brelse(bh_rhs);

    return 0;
}

static inline int basicbtfs_btree_node_steal_from_previous(struct super_block *sb, uint32_t bno, int index) {
    struct buffer_head *bh_par = NULL, *bh_lhs = NULL, *bh_rhs = NULL;
    struct basicbtfs_btree_node *node = NULL, *lhs = NULL, *rhs = NULL;
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    node = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

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
    int i = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    node = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

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
    int ret = 0;

    bh_par = sb_bread(sb, bno);

    if (!bh_par) return -EIO;

    node = (struct basicbtfs_btree_node *) bh_par->b_data;

    bh_lhs = sb_bread(sb, node->children[index]);

    if (!bh_lhs) {
        brelse(bh_par);
        return -EIO;
    }

    lhs = (struct basicbtfs_btree_node *) bh_lhs->b_data;

    bh_rhs = sb_bread(sb, node->children[index + 1]);

    if (!bh_rhs) {
        brelse(bh_par);
        brelse(bh_lhs);
        return -EIO;
    }

    rhs = (struct basicbtfs_btree_node *) bh_rhs->b_data;

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

static inline int basicbtfs_btree_node_delete(struct super_block *sb, uint32_t bno, char *filename) {
    struct buffer_head *bh = NULL, *bh2 = NULL;
    struct basicbtfs_btree_node *node = NULL, *child = NULL;
    int index = basicbtfs_btree_node_find_key(sb, bno, filename);
    int ret = 0;
    bool flag = false;
    uint32_t bno2 = 0;

    bno2 = basicbtfs_btree_node_lookup(sb, bno, filename, 0);

    if (bno2 != 0 && bno2 != 1) {
        printk("yes we did it\n");
    }

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    if (index < node->nr_of_keys && strncmp(node->entries[index].hash_name, filename,BASICBTFS_NAME_LENGTH) == 0) {
        if (node->leaf) {
            ret = basicbtfs_btree_node_remove_from_leaf(sb, bno, index);
        } else {
            ret = basicbtfs_btree_node_remove_from_nonleaf(sb, bno, index);
        }
    } else {
        if (node->leaf) {
            printk(KERN_INFO "File %s doesn't exist with index %d\n", filename, index);
        }

        bh2 = sb_bread(sb, node->children[index]);

        if (!bh2) {
            brelse(bh);
            return -EIO;
        }

        child = (struct basicbtfs_btree_node *) bh2->b_data;

        flag = (index == node->nr_of_keys) ? true : false;

        if (child->nr_of_keys < BASICBTFS_MIN_DEGREE) {
            ret = basicbtfs_btree_node_fill(sb, bno, index);

            if (ret != 0) {
                brelse(bh);
                brelse(bh2);
                return ret;
            }
        }

        basicbtfs_btree_node_delete(sb, node->children[index], filename);
        brelse(bh2);
    }
    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_delete_entry(struct super_block *sb, struct inode *inode, uint32_t root_bno, char *filename) {
    struct buffer_head *bh = NULL, *bh2 = NULL;
    struct basicbtfs_btree_node *node, *new_root_node = NULL;
    int ret = 0;

    bh = sb_bread(sb, root_bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    ret = basicbtfs_btree_node_delete(sb, root_bno, filename);

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

            new_root_node = (struct basicbtfs_btree_node *) bh2->b_data;
            new_root_node->nr_of_files = node->nr_of_files - 1;
            new_root_node->nr_times_done = node->nr_times_done;
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

static inline int basicbtfs_btree_traverse(struct super_block *sb, uint32_t bno, struct dir_context *ctx, loff_t start_ctx, loff_t *ctx_index) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int index = 0, ret = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_btree_traverse(sb, node->children[index], ctx, start_ctx, ctx_index);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }
        if (*ctx_index < start_ctx) {
            *ctx_index += 1;
            continue;
        }

        // printk(KERN_INFO "file: %s | ino: %d\n", node->entries[index].hash_name, node->entries[index].ino);

        if (!dir_emit(ctx, node->entries[index].hash_name, BASICBTFS_NAME_LENGTH, node->entries[index].ino, DT_UNKNOWN)) {
            printk(KERN_INFO "No files available anymore\n");
            break;
        }


        ctx->pos++;
        *ctx_index = *ctx_index + 1;
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_traverse(sb, node->children[index], ctx, start_ctx, ctx_index);
    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_btree_traverse_debug(struct super_block *sb, uint32_t bno) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    int index = 0, ret = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    node = (struct basicbtfs_btree_node *) bh->b_data;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_btree_traverse_debug(sb, node->children[index]);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }
        printk(KERN_INFO "file: %s | ino: %d\n", node->entries[index].hash_name, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_traverse_debug(sb, node->children[index]);
    }

    brelse(bh);
    return 0;
}

#endif

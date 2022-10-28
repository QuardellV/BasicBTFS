#ifndef BASICBTFS_BTREECACHE_H
#define BASICBTFS_BTREECACHE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicbtfs.h"
#include "bitmap.h"
#include "cache.h"

static inline int basicbtfs_btree_node_cache_delete(struct super_block *sb, struct basicbtfs_btree_node_cache *node, uint32_t hash, struct inode *inode);

static inline void basicbtfs_btree_node_cache_init(struct super_block *sb, struct basicbtfs_btree_node_cache *node, bool leaf) {
    memset(node, 0, sizeof(struct basicbtfs_btree_node_cache));
    node->nr_of_keys = 0;
    node->leaf = leaf;
}

static inline int basicbtfs_btree_cache_update_root(struct inode *inode, uint32_t old_bno, struct basicbtfs_btree_node_cache *node_cache) {
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);

    uint32_t ino = inode->i_ino;

    if (ino >= sbi->s_ninodes) return -1;
    basicbtfs_cache_update_root_node(inode_info->i_bno, node_cache);
    return 0;
}

static inline uint32_t basicbtfs_btree_node_cache_lookup_with_entry(struct super_block *sb, struct basicbtfs_btree_node_cache *btr_node, uint32_t hash, int counter, struct basicbtfs_entry *entry) {
    uint32_t ret = 0;
    int index = 0;

    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }

    if (btr_node->entries[index].hash == hash) {
        ret = btr_node->entries[index].ino;
        memcpy(entry, &btr_node->entries[index], sizeof(struct basicbtfs_entry));
        return ret;
    }

    if (btr_node->leaf) {
        return 0;
    }
    ret = basicbtfs_btree_node_cache_lookup_with_entry(sb, btr_node->children[index], hash, counter, entry);
    return ret;
}

static inline int basicbtfs_btree_cache_split_child(struct super_block *sb, struct basicbtfs_btree_node_cache *node_par, struct basicbtfs_btree_node_cache *node_lhs, int index, struct inode *inode, uint32_t old_bno) {
    struct basicbtfs_btree_node_cache *node_rhs = NULL;
    int i = 0;

    node_rhs = (struct basicbtfs_btree_node_cache *) basicbtfs_alloc_file(sb);

    basicbtfs_btree_node_cache_init(sb, node_rhs, node_lhs->leaf);
    node_rhs->nr_of_keys = BASICBTFS_MIN_DEGREE - 1;
    basicbtfs_cache_add_node(sb, old_bno, node_rhs);

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

    node_par->children[index+1] = node_rhs;

    for (i = node_par->nr_of_keys - 1; i >= index; i--) {
        memcpy(&node_par->entries[i + 1], &node_par->entries[i], sizeof(struct basicbtfs_entry));
    }

    memcpy(&node_par->entries[index], &node_lhs->entries[BASICBTFS_MIN_DEGREE - 1], sizeof(struct basicbtfs_entry));
    node_par->nr_of_keys++;

    return 0;
}

static inline int basicbtfs_btree_cache_insert_non_full(struct super_block *sb, struct basicbtfs_btree_node_cache *node, struct basicbtfs_entry *new_entry, struct inode *inode, uint32_t old_bno) {
    struct basicbtfs_btree_node_cache *child = NULL;
    int ret = 0;

    if (node->leaf) {
        int index = node->nr_of_keys - 1;
        while (index >= 0 && node->entries[index].hash > new_entry->hash) {
            memcpy(&node->entries[index + 1], &node->entries[index], sizeof(struct basicbtfs_entry));
            index--;
        }

        memcpy(&node->entries[index + 1], new_entry, sizeof(struct basicbtfs_entry));
        node->nr_of_keys++;
    } else {
        int index = node->nr_of_keys - 1;
        while (index >= 0 && node->entries[index].hash > new_entry->hash) {
            index--;
        }

        child = node->children[index + 1];

        if (child->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE - 1) {
            ret = basicbtfs_btree_cache_split_child(sb, node, node->children[index + 1], index + 1, inode, old_bno);

            if (ret != 0) {
                return ret;
            }

            if (node->entries[index+1].hash < new_entry->hash) {
                index++;
            }
        }

        basicbtfs_btree_cache_insert_non_full(sb, node->children[index + 1], new_entry, inode, old_bno);
    }

    return 0;
}

static inline int basicbtfs_btree_node_cache_insert(struct super_block *sb, struct inode *par_inode, struct basicbtfs_btree_node_cache *old_node, struct basicbtfs_entry *entry, uint32_t old_bno) {
    struct basicbtfs_btree_node_cache *new_node = NULL;
    int ret = 0;

    if (old_node->nr_of_keys == 2 * BASICBTFS_MIN_DEGREE -1) {
        int index = 0;

        new_node = (struct basicbtfs_btree_node_cache *)basicbtfs_alloc_file(sb);
        basicbtfs_btree_node_cache_init(sb, new_node, false);
        new_node->children[0] = old_node;
        basicbtfs_cache_add_node(sb, old_bno, new_node);

        ret = basicbtfs_btree_cache_split_child(sb, new_node, old_node, 0, par_inode, old_bno);

        if (ret != 0) {
            return ret;
        }

        if (new_node->entries[index].hash < entry->hash) {
            index++;
        }

        ret = basicbtfs_btree_cache_insert_non_full(sb, new_node->children[index], entry, par_inode, old_bno);

        if (ret != 0) {
            return ret;
        }

        ret = basicbtfs_btree_cache_update_root(par_inode, old_bno, new_node);

        if (ret != 0) {
            return ret;
        }

        new_node->nr_of_files = old_node->nr_of_files + 1;
        new_node->nr_times_done = old_node->nr_times_done + 1;
        new_node->tree_name_bno = old_node->tree_name_bno;
    } else {
        ret = basicbtfs_btree_cache_insert_non_full(sb, old_node, entry, par_inode, old_bno);

        if (ret != 0) {
            return ret;
        }
        old_node->nr_of_files++;
        old_node->nr_times_done++;
    }
    return 0;
}

static inline int basicbtfs_btree_node_cache_update(struct super_block *sb, struct basicbtfs_btree_node_cache *btr_node, uint32_t hash, int counter, uint32_t inode) {
    uint32_t ret = 0;
    int index = 0;

    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }

    if (btr_node->entries[index].hash == hash) {
        btr_node->entries[index].ino = inode;
        return ret;
    }

    if (btr_node->leaf) {
        return 0;
    }
    ret =  basicbtfs_btree_node_cache_update(sb, btr_node->children[index], hash, counter, inode);
    return ret;
}

static inline int basicbtfs_btree_node_cache_find_key(struct super_block *sb, struct basicbtfs_btree_node_cache *node, uint32_t hash) {

    int index = 0;

    while (index < node->nr_of_keys && node->entries[index].hash < hash) {
        ++index;
    }

    return index;
}

static inline int basicbtfs_btree_node_cache_remove_from_leaf(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index) {
    int i = 0;

    for (i = index + 1; i < node->nr_of_keys; i++) {
        memcpy(&node->entries[i - 1], &node->entries[i], sizeof(struct basicbtfs_entry));
    }

    node->nr_of_keys--;
    return 0;
}

static inline int basicbtfs_btree_node_cache_get_predecessor(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index, struct basicbtfs_entry *ret) {
    struct basicbtfs_btree_node_cache *child = node->children[index];

    while (!node->leaf) {
        child = node->children[node->nr_of_keys];
    }

    memcpy(ret, &node->entries[node->nr_of_keys- 1], sizeof (struct basicbtfs_entry));
    return 0;
}

static inline int basicbtfs_btree_node_cache_get_successor(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index, struct basicbtfs_entry *ret) {
    struct basicbtfs_btree_node_cache *child = node->children[index + 1];

    while (!node->leaf) {
        child = node->children[0];
    }

    memcpy(ret, &node->entries[0], sizeof (struct basicbtfs_entry));
    return 0;
}

static inline int basicbtfs_btree_node_cache_merge(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index, struct inode *inode) {
    struct basicbtfs_btree_node_cache *lhs = NULL, *rhs = NULL;
    int i = 0;

    lhs = node->children[index];
    rhs = node->children[index + 1];

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
    basicbtfs_cache_delete_node(sb, BASICBTFS_INODE(inode)->i_bno, rhs);

    return 0;
}

static inline int basicbtfs_btree_node_cache_remove_from_nonleaf(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index, struct inode *inode) {
    struct basicbtfs_btree_node_cache *lhs = NULL, *rhs = NULL;
    struct basicbtfs_entry tmp, pred, succ;
    int ret = 0;

    lhs = node->children[index];
    rhs = node->children[index + 1];

    if (lhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_cache_get_predecessor(sb, node, index, &pred);

        if (ret != 0) {
            return ret;
        }
        memcpy(&node->entries[index], &pred, sizeof(struct basicbtfs_entry));
        ret = basicbtfs_btree_node_cache_delete(sb, node->children[index], pred.hash, inode);
    } else if (rhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_cache_get_successor(sb, node, index, &succ);

        if (ret != 0) {
            return ret;
        }

        memcpy(&node->entries[index], &succ, sizeof(struct basicbtfs_entry));
        ret = basicbtfs_btree_node_cache_delete(sb, node->children[index + 1], succ.hash, inode);
    } else {
        memcpy(&tmp, &node->entries[index], sizeof(struct basicbtfs_entry));
        ret = basicbtfs_btree_node_cache_merge(sb, node, index, inode);

        if (ret != 0) {
            return ret;
        }

        ret = basicbtfs_btree_node_cache_delete(sb, node->children[index], tmp.hash, inode);
    }

    return 0;
}

static inline int basicbtfs_btree_node_cache_steal_from_previous(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index) {
    struct basicbtfs_btree_node_cache *lhs = NULL, *rhs = NULL;
    int i = 0;

    lhs = node->children[index];
    rhs = node->children[index + 1];

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

    return 0;
}

static inline int basicbtfs_btree_node_cache_steal_from_next(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index) {
    struct basicbtfs_btree_node_cache *lhs = NULL, *rhs = NULL;
    int i = 0;

    lhs = node->children[index];
    rhs = node->children[index + 1];

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

    return 0;
}

static inline int basicbtfs_btree_node_cache_fill(struct super_block *sb, struct basicbtfs_btree_node_cache *node, int index, struct inode *inode) {
    struct basicbtfs_btree_node_cache *lhs = NULL, *rhs = NULL;
    int ret = 0;

    lhs = node->children[index];
    rhs = node->children[index + 1];

    if (index != 0 && lhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_cache_steal_from_previous(sb, node, index);
    } else if (index != node->nr_of_keys && rhs->nr_of_keys >= BASICBTFS_MIN_DEGREE) {
        ret = basicbtfs_btree_node_cache_steal_from_next(sb, node, index);
    } else {
        if (index != node->nr_of_keys) {
            ret = basicbtfs_btree_node_cache_merge(sb, node, index, inode);
        } else {
            ret = basicbtfs_btree_node_cache_merge(sb, node, index - 1, inode);
        }
    }

    return 0;
}

static inline int basicbtfs_btree_node_cache_delete(struct super_block *sb, struct basicbtfs_btree_node_cache *node, uint32_t hash, struct inode *inode) {
    struct basicbtfs_btree_node_cache *child = NULL;
    int index = basicbtfs_btree_node_cache_find_key(sb, node, hash);
    int ret = 0;
    bool flag = false;

    if (index < node->nr_of_keys && node->entries[index].hash == hash) {
        if (node->leaf) {
            ret = basicbtfs_btree_node_cache_remove_from_leaf(sb, node, index);
        } else {
            ret = basicbtfs_btree_node_cache_remove_from_nonleaf(sb, node, index, inode);
        }
    } else {
        if (node->leaf) {
            //TODO: Fix this
            printk(KERN_INFO "File %d doesn't exist with index %d\n", hash, index);
        }

        child = node->children[index];

        flag = (index == node->nr_of_keys) ? true : false;

        if (child->nr_of_keys < BASICBTFS_MIN_DEGREE) {
            ret = basicbtfs_btree_node_cache_fill(sb, node, index, inode);

            if (ret != 0) {

                return ret;
            }
        }

        basicbtfs_btree_node_cache_delete(sb, node->children[index], hash, inode);
    }
    return 0;
}

static inline int basicbtfs_btree_cache_delete_entry(struct super_block *sb, struct inode *inode, struct basicbtfs_btree_node_cache *node, uint32_t hash, uint32_t old_bno) {
    struct basicbtfs_btree_node_cache *new_root_node = NULL;
    int ret = 0;

    ret = basicbtfs_btree_node_cache_delete(sb, node, hash, inode);

    if (ret == 1) {
        return 0;
    } else if (ret < 0) {
        return ret;
    }

    if (node->nr_of_keys == 0) {
        if (node->leaf) {
            node->nr_of_files--;
        } else {
            ret = basicbtfs_btree_cache_update_root(inode, old_bno, node->children[0]);

            if (ret != 0) {
                return ret;
            }

            new_root_node = node->children[0];
            new_root_node->nr_of_files = node->nr_of_files - 1;
            new_root_node->nr_times_done = node->nr_times_done;
            new_root_node->tree_name_bno = node->tree_name_bno;
            basicbtfs_cache_delete_node(sb, BASICBTFS_INODE(inode)->i_bno, node);
        }
    } else {
        node->nr_of_files--;
    }

    return ret;
}

static inline int basicbtfs_btree_cache_traverse_debug(struct super_block *sb, struct basicbtfs_btree_node_cache *node) {
    int index = 0, ret = 0;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_btree_cache_traverse_debug(sb, node->children[index]);

            if (ret != 0) {
                return ret;
            }
        }
    }

    if (!node->leaf) {
        ret = basicbtfs_btree_cache_traverse_debug(sb, node->children[index]);
    }
    return 0;
}

#endif

#ifndef BASICBTFS_CACHE_H
#define BASICBTFS_CACHE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>

#include "basicbtfs.h"
#include "bitmap.h"

static inline void basicbtfs_cache_add_dir(struct super_block *sb, uint32_t bno, struct basicbtfs_btree_node *node) {
    struct basicbtfs_btree_dir_cache_list *new_cache_dir_entry = basicbtfs_alloc_btree_dir(sb);
    new_cache_dir_entry->btree_dir_cache = basicbtfs_alloc_btree_node_hdr(sb);
    new_cache_dir_entry->bno = bno;
    INIT_LIST_HEAD(&new_cache_dir_entry->list);
    list_add(&new_cache_dir_entry->list, &dir_cache_list);

    new_cache_dir_entry->btree_dir_cache->node = basicbtfs_alloc_btree_node_data(sb);
    memcpy(new_cache_dir_entry->btree_dir_cache->node, node, sizeof(struct basicbtfs_btree_node));


    new_cache_dir_entry->btree_dir_cache->bno = bno;
    INIT_LIST_HEAD(&new_cache_dir_entry->btree_dir_cache->list);
    list_add(&new_cache_dir_entry->btree_dir_cache->list, &new_cache_dir_entry->btree_dir_cache->list);
}

static inline void basicbtfs_cache_add_node(struct super_block *sb, uint32_t hash, uint32_t bno, struct basicbtfs_btree_node *node) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;

    struct basicbtfs_btree_node_hdr_cache *node_hdr_cache = basicbtfs_alloc_btree_node_hdr(sb);
    node_hdr_cache->node = basicbtfs_alloc_btree_node_data(sb);
    memcpy(node_hdr_cache->node, node, sizeof(struct basicbtfs_btree_node));
    node_hdr_cache->bno = bno;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            INIT_LIST_HEAD(&node_hdr_cache->list);
            list_add(&node_hdr_cache->list, &dir_cache->btree_dir_cache->list);
            break;
        }
    }
}

static inline void basicbtfs_cache_update_node(struct super_block *sb, uint32_t hash, uint32_t bno, struct basicbtfs_btree_node *node) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_hdr_cache *node_hdr_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            list_for_each_entry(node_hdr_cache, &dir_cache->btree_dir_cache->list, list) {
                if (node_hdr_cache->bno == bno) {
                    memcpy(node_hdr_cache->node, node, sizeof(struct basicbtfs_btree_node));
                    break;
                }
            }
        }
    }
}

static inline void basicbtfs_cache_update_root_node(struct super_block *sb, uint32_t hash, uint32_t new_bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            dir_cache->bno = new_bno;
        }
    }
}

static inline void basicbtfs_cache_delete_dir(struct super_block *sb, uint32_t hash) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_hdr_cache *node_hdr_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            list_for_each_entry(node_hdr_cache, &dir_cache->btree_dir_cache->list, list) {
                list_del(&node_hdr_cache->list);
                basicbtfs_destroy_btree_node_data(node_hdr_cache->node);
                basicbtfs_destroy_btree_node_hdr(node_hdr_cache);
            }
            list_del(&dir_cache->list);
            basicbtfs_destroy_btree_dir(dir_cache);
            break;
        }
    }
}

static inline void basicbtfs_cache_delete_node(struct super_block *sb, uint32_t hash, uint32_t bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_hdr_cache *node_hdr_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            list_for_each_entry(node_hdr_cache, &dir_cache->btree_dir_cache->list, list) {
                if (node_hdr_cache->bno == bno) {
                    list_del(&node_hdr_cache->list);
                    basicbtfs_destroy_btree_node_data(node_hdr_cache->node);
                    basicbtfs_destroy_btree_node_hdr(node_hdr_cache);
                    return;
                }
            }
        }
    }

}

#endif
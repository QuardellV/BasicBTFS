#ifndef BASICBTFS_CACHE_H
#define BASICBTFS_CACHE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>

#include "basicbtfs.h"
#include "bitmap.h"

struct list_head dir_cache_list = { &dir_cache_list, &dir_cache_list};

static inline void basicbtfs_cache_add_dir(struct super_block *sb, uint32_t hash, uint32_t bno, struct basicbtfs_btree_node *node) {
    struct basicbtfs_btree_dir_cache_list *new_cache_dir_entry = basicbtfs_alloc_btree_dir(sb);
    new_cache_dir_entry->btree_dir_cache = basicbtfs_alloc_btree_node_hdr(sb);
    new_cache_dir_entry->hash = hash;
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
        if (dir_cache->hash == hash) {
            INIT_LIST_HEAD(&node_hdr_cache->list);
            list_add(&node_hdr_cache->list, dir_cache->btree_dir_cache->list);
            break;
        }
    }
}

static inline void basicbtfs_cache_update_node(struct super_block *sb, uint32_t hash, uint32_t bno, struct basicbtfs_btree_node *node) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_hdr_cache *node_hdr_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->hash == hash) {
            list_for_each_entry(node_hdr_cache, &dir_cache->btree_dir_cache->list, list) {
                if (node_hdr_cache->bno == bno) {
                    memcpy(node_hdr_cache->node, node, sizeof(struct basicbtfs_btree_node));
                    break;
                }
            }
        }
    }
    
}

#endif
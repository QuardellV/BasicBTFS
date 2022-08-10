#ifndef BASICBTFS_CACHE_H
#define BASICBTFS_CACHE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>

#include "basicbtfs.h"
#include "bitmap.h"
// #include "nametree.h"

static inline void basicbtfs_cache_add_dir(struct super_block *sb, uint32_t bno, struct basicbtfs_btree_node_cache *node, struct basicbtfs_block *name_block, uint32_t name_bno) {
    struct basicbtfs_btree_dir_cache_list *new_cache_dir_entry = basicbtfs_alloc_btree_dir(sb);
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_name_tree_cache *name_tree_cache;
    // struct basicbtfs_block *tmp_block = basicbtfs_alloc_file(sb);
    new_cache_dir_entry->root_node_cache = (struct basicbtfs_btree_node_cache *)basicbtfs_alloc_file(sb);
    new_cache_dir_entry->bno = bno;
    new_cache_dir_entry->nr_of_blocks = 5;
    INIT_LIST_HEAD(&new_cache_dir_entry->list);
    list_add(&new_cache_dir_entry->list, &dir_cache_list);

    new_cache_dir_entry->name_tree_cache = basicbtfs_alloc_nametree_hdr(sb);
    name_tree_cache = basicbtfs_alloc_nametree_hdr(sb);

    new_cache_dir_entry->name_tree_cache->name_tree_block = basicbtfs_alloc_file(sb);
    name_tree_cache->name_tree_block = basicbtfs_alloc_file(sb);

    name_tree_cache->name_bno = name_bno;
    memcpy(name_tree_cache->name_tree_block, name_block, sizeof(struct basicbtfs_block));

    INIT_LIST_HEAD(&new_cache_dir_entry->name_tree_cache->list);
    INIT_LIST_HEAD(&name_tree_cache->list);
    list_add(&name_tree_cache->list, &new_cache_dir_entry->name_tree_cache->list);

    // list_add(&new_cache_dir_entry->name_tree_cache->list, &new_cache_dir_entry->name_tree_cache->list);
    INIT_LIST_HEAD(&new_cache_dir_entry->root_node_cache->list);
    INIT_LIST_HEAD(&node->list);
    list_add(&node->list, &new_cache_dir_entry->root_node_cache->list);

    if (list_empty(&new_cache_dir_entry->root_node_cache->list)) {
        printk("empty\n");
    } else {
        printk("not empty\n");
    }
    sbi->s_cache_dir_entries++;
}

static inline void basicbtfs_cache_add_node(struct super_block *sb, uint32_t dir_bno, struct basicbtfs_btree_node_cache *node) {
    struct basicbtfs_btree_dir_cache_list *dir_cache, *tmp;

    list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            INIT_LIST_HEAD(&node->list);
            list_add_tail(&node->list, &dir_cache->root_node_cache->list);

            list_del(&dir_cache->list);
            dir_cache->nr_of_blocks++;
            list_add(&dir_cache->list, &dir_cache_list);
            break;
        }
    }
}

static inline void basicbtfs_cache_add_name_block(struct super_block *sb, uint32_t dir_bno, struct basicbtfs_block *name_block, uint32_t name_bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache, *tmp;

    printk("add name block\n");

    struct basicbtfs_name_tree_cache *nametree_hdr = basicbtfs_alloc_nametree_hdr(sb);
    nametree_hdr->name_tree_block = basicbtfs_alloc_file(sb);
    nametree_hdr->name_bno = name_bno;

    memcpy(nametree_hdr->name_tree_block, name_block, sizeof(struct basicbtfs_block));

    list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            INIT_LIST_HEAD(&nametree_hdr->list);
            list_add(&nametree_hdr->list, &dir_cache->name_tree_cache->list);

            list_del(&dir_cache->list);
            dir_cache->nr_of_blocks++;
            list_add(&dir_cache->list, &dir_cache_list);
            break;
        }
    }
}

// static inline void basicbtfs_cache_update_node(struct super_block *sb, uint32_t hash, uint32_t bno, struct basicbtfs_btree_node *node) {
//     struct basicbtfs_btree_dir_cache_list *dir_cache;
//     struct basicbtfs_btree_node_hdr_cache *node_hdr_cache;

//     list_for_each_entry(dir_cache, &dir_cache_list, list) {
//         if (dir_cache->bno == hash) {
//             list_for_each_entry(node_hdr_cache, &dir_cache->btree_dir_cache->list, list) {
//                 if (node_hdr_cache->bno == bno) {
//                     memcpy(node_hdr_cache->node, node, sizeof(struct basicbtfs_btree_node));
//                     break;
//                 }
//             }
//         }
//     }
// }

static inline void basicbtfs_cache_update_block(struct super_block *sb, uint32_t dir_bno, struct basicbtfs_block *name_block, uint32_t name_bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_name_tree_cache *nametree_hdr_cache, *tmp;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            list_for_each_entry_safe(nametree_hdr_cache, tmp, &dir_cache->name_tree_cache->list, list) {
                if (nametree_hdr_cache->name_bno == name_bno) {
                    memcpy(nametree_hdr_cache->name_tree_block, name_block, sizeof(struct basicbtfs_block));
                    break;
                }
            }
        }
    }
}

static inline void basicbtfs_cache_update_root_node(uint32_t dir_bno, struct basicbtfs_btree_node_cache *new_node) {
    struct basicbtfs_btree_dir_cache_list *tmp, *dir_cache;
    struct basicbtfs_btree_node_cache *node_cache;

    list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            printk("found\n");
            list_del(&new_node->list);
            list_add(&new_node->list, &dir_cache->root_node_cache->list);
            break;
        }
    }

    node_cache =  list_first_entry(&dir_cache->root_node_cache->list, struct basicbtfs_btree_node_cache, list);
    if (new_node == node_cache) {
        printk("si\n");
    } else {
        printk("not the same\n");
    }
}

static inline void basicbtfs_cache_update_root_bno(uint32_t dir_bno, uint32_t new_bno) {
    struct basicbtfs_btree_dir_cache_list *tmp, *dir_cache;
    printk("new bno: %d\n", new_bno);
    bool found = false;

    list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            list_del(&dir_cache->list);
            dir_cache->bno = new_bno;
            list_add(&dir_cache->list, &dir_cache_list);
        }
    }
}

static inline struct basicbtfs_btree_node_cache * basicbtfs_cache_get_root_node(uint32_t dir_bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_cache *node_cache;

    printk("current dir_bno: %d\n", dir_bno);

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        printk("checked bno: %d\n", dir_cache->bno);
        if (dir_cache->bno == dir_bno) {
            printk("found new bno: %d\n", dir_bno);
            node_cache =  list_first_entry(&dir_cache->root_node_cache->list, struct basicbtfs_btree_node_cache, list);
            return node_cache;
        }
    }

    printk("no :(\n");
    return NULL;
}

static inline uint32_t basicbtfs_cache_get_nr_of_blocks(uint32_t dir_bno) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_cache *node_cache;

    printk("current dir_bno: %d\n", dir_bno);

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        printk("checked bno: %d\n", dir_cache->bno);
        if (dir_cache->bno == dir_bno) {
            return dir_cache->nr_of_blocks;
        }
    }

    printk("no :(\n");
    return 0;
}



static inline void basicbtfs_cache_delete_dir(struct super_block *sb, uint32_t hash) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_btree_dir_cache_list *dir_cache, *tmp_dir_cache;
    struct basicbtfs_btree_node_cache *node_cache, *tmp_node;
    struct basicbtfs_name_tree_cache *nametree_hdr_cache, *tmp_natr_hdr_cache;

    list_for_each_entry_safe(dir_cache, tmp_dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            printk("found\n");
            list_for_each_entry_safe(node_cache, tmp_node, &dir_cache->root_node_cache->list, list) {
                printk("yes\n");
                list_del(&node_cache->list);
                basicbtfs_destroy_file_block((struct basicbtfs_block *) node_cache);
                // basicbtfs_destroy_btree_node_data(node_cache);
            }

            list_for_each_entry_safe(nametree_hdr_cache, tmp_natr_hdr_cache, &dir_cache->name_tree_cache->list, list) {
                list_del(&nametree_hdr_cache->list);
                basicbtfs_destroy_file_block(nametree_hdr_cache->name_tree_block);
                basicbtfs_destroy_nametree_hdr(nametree_hdr_cache);
            }
            list_del(&dir_cache->list);
            basicbtfs_destroy_btree_dir(dir_cache);
            sbi->s_cache_dir_entries--;
            break;
        }
    }
}

static inline int basicbtfs_cache_emit_block(struct basicbtfs_block *btfs_block, int *total_nr_entries, int *current_index, struct dir_context *ctx, loff_t start_pos) {
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    char *block = NULL;
    char *filename = NULL;
    int i = 0;
    
    name_tree = (struct basicbtfs_name_tree *) btfs_block;
    block = (char *) btfs_block;
    block += sizeof(struct basicbtfs_name_tree);
    cur_entry = (struct basicbtfs_name_entry *) block;
    *total_nr_entries += name_tree->nr_of_entries;

    if (start_pos < *total_nr_entries) {
        for (i = 0; i < name_tree->nr_of_entries; i++) {
            block += sizeof(struct basicbtfs_name_entry);
            if (cur_entry->ino != 0) {
                filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
                strncpy(filename, block, cur_entry->name_length);
                if (!dir_emit(ctx, filename, cur_entry->name_length - 1, cur_entry->ino, DT_UNKNOWN)) {
                    printk(KERN_INFO "No files available anymore\n");
                    kfree(filename);
                    return 1;
                }
                ctx->pos++;
                kfree(filename);
            } else {
                i--;
            }

            block += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
            // *current_index++;
        }
    }

    return 0;
}

static inline bool basicbtfs_cache_iterate_dir(struct super_block *sb, uint32_t hash, struct dir_context *ctx, loff_t start_pos) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_name_tree_cache *nametree_hdr_cache;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            list_for_each_entry(nametree_hdr_cache, &dir_cache->name_tree_cache->list, list) {
                basicbtfs_cache_emit_block(nametree_hdr_cache->name_tree_block, &total_nr_entries, &current_index, ctx, start_pos);
            }

            return true;
        }
    }
    return false;
}

static inline int basicbtfs_cache_emit_block_debug(struct basicbtfs_block *btfs_block, int *total_nr_entries, int *current_index) {
    struct basicbtfs_name_tree *name_tree = NULL;
    struct basicbtfs_name_entry *cur_entry = NULL;
    char *block = NULL;
    char *filename = NULL;
    int i = 0;
    
    name_tree = (struct basicbtfs_name_tree *) btfs_block;
    block = (char *) btfs_block;
    block += sizeof(struct basicbtfs_name_tree);
    cur_entry = (struct basicbtfs_name_entry *) block;
    *total_nr_entries += name_tree->nr_of_entries;

    printk("nr of entries: %d\n", name_tree->nr_of_entries);

    for (i = 0; i < name_tree->nr_of_entries; i++) {
        block += sizeof(struct basicbtfs_name_entry);
        if (cur_entry->ino != 0) {
            filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            printk("current filename: %s\n", filename);
            kfree(filename);
        } else {
            i--;
        }

        block += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
        // *current_index++;
    }

    printk("current index: %d\n", *current_index);
    return 0;
}

static inline bool basicbtfs_cache_iterate_dir_debug(struct super_block *sb, uint32_t hash) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_name_tree_cache *nametree_hdr_cache;
    uint32_t current_index = 0;
    uint32_t total_nr_entries = 0;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == hash) {
            printk("start operation\n");
            list_for_each_entry(nametree_hdr_cache, &dir_cache->name_tree_cache->list, list) {
                printk("iterate\n");
                basicbtfs_cache_emit_block_debug(nametree_hdr_cache->name_tree_block, &total_nr_entries, &current_index);
            }

            return true;
        }
    }
    return false;
}

static inline uint32_t basicbtfs_btree_node_cache_lookup(struct basicbtfs_btree_node_cache *btr_node, uint32_t hash, int counter) {
    uint32_t ret = 0;
    int index = 0;

    // hash > btr_node->entries[index].hash
    while (index < btr_node->nr_of_keys && hash > btr_node->entries[index].hash) {
        index++;
        counter++;
    }
    // btr_node->entries[index].hash == hash
    if (btr_node->entries[index].hash == hash) {
        printk(KERN_INFO "Current counter: %d of %d\n", counter, hash);
        ret = btr_node->entries[index].ino;
        return ret;
    }

    if (btr_node->leaf) {
        return 0;
    }

    ret = basicbtfs_btree_node_cache_lookup(btr_node->children[index], hash, counter);
    return ret;
}

static inline uint32_t basicbtfs_cache_lookup_entry(struct super_block *sb, uint32_t dir_bno, uint32_t hash) {
    struct basicbtfs_btree_dir_cache_list *dir_cache;
    struct basicbtfs_btree_node_cache *node_cache;

    list_for_each_entry(dir_cache, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            printk("found in lookup cache\n");
            node_cache =  list_first_entry(&dir_cache->root_node_cache->list, struct basicbtfs_btree_node_cache, list);
            return basicbtfs_btree_node_cache_lookup(node_cache, hash, 0);
        }
    }

    return 0;
}

static inline void basicbtfs_cache_delete_node(struct super_block *sb, uint32_t dir_bno, struct basicbtfs_btree_node_cache *node_cache) {
    struct basicbtfs_btree_dir_cache_list *dir_cache, *tmp;
    list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
        if (dir_cache->bno == dir_bno) {
            list_del(&dir_cache->list);
            dir_cache->nr_of_blocks--;
            list_add(&dir_cache->list, &dir_cache_list);

            list_del(&node_cache->list);
            basicbtfs_destroy_file_block((struct basicbtfs_block *) node_cache);
            break;
        }
    }

    // basicbtfs_destroy_btree_node_data(node_cache);
}

// static inline void basicbtfs_cache_delete_name_block(struct super_block *sb, uint32_t dir_bno, struct basicbtfs_btree_node_cache *node_cache) {
//     struct basicbtfs_btree_dir_cache_list *dir_cache, *tmp;
//     list_for_each_entry_safe(dir_cache, tmp, &dir_cache_list, list) {
//         if (dir_cache->bno == dir_bno) {
//             list_del(&dir_cache->list);
//             dir_cache->nr_of_blocks--;
//             list_add(&dir_cache->list, &dir_cache_list);
//             list_del(&node_cache->list);
//             basicbtfs_destroy_file_block((struct basicbtfs_block *) node_cache);
//             break;
//         }
//     }

//     // basicbtfs_destroy_btree_node_data(node_cache);
// }

#endif
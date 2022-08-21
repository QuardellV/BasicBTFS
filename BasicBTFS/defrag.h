#ifndef BASICBTFS_DEFRAG_H
#define BASICBTFS_DEFRAG_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "basicbtfs.h"
#include "btree.h"
#include "nametree.h"
#include "bitmap.h"
#include "cache.h"
#include "init.h"

static inline void basicbtfs_defrag_directory(struct super_block *sb, struct inode *inode, uint32_t *offset);
static inline int basicbtfs_defrag_move_file_block(struct super_block *sb, struct buffer_head *bh_old, uint32_t new_bno);
static inline void basicbtfs_defrag_move_cluster_table(struct super_block *sb, struct buffer_head *bh, uint32_t new_bno);
static inline int basicbtfs_defrag_move_namelist(struct super_block *sb, struct buffer_head *bh,  uint32_t new_bno);
static inline int basicbtfs_defrag_move_btree_node(struct super_block *sb, struct buffer_head *bh, uint32_t cur_bno, uint32_t new_bno);


static inline int basicbtfs_defrag_btree_node(struct super_block *sb, uint32_t old_bno, uint32_t *offset) {
    /**
     * 1. If root, move btree and update inode->i_bno on disk and cache
     * 2. if non-root, move btree and update parent reference
     */
    struct buffer_head *bh_old = NULL, *bh_new, *bh_swap;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_disk_block *disk_block_swap, *disk_block_old, *disk_block_new;
    uint32_t tmp_bno, new_bno;

    printk("defrag btree nodee and offset: %d | %d\n", old_bno, *offset);

    if (old_bno == *offset) {
        printk("It is already placed on the correct place. No further action\n");
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1)) {
        printk("The new block is empty. This will be taken and the old block will be removed\n");
        new_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1);

        bh_old = sb_bread(sb, old_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, new_bno);

        if (!bh_new) {
            brelse(bh_old);
            return -EIO;
        }

        disk_block_old = (struct basicbtfs_disk_block *) bh_old->b_data;
        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;


        memcpy(disk_block_new, disk_block_old, BASICBTFS_BLOCKSIZE);
        basicbtfs_defrag_move_btree_node(sb, bh_new, old_bno, *offset);
        mark_buffer_dirty(bh_new);
        put_blocks(sbi, old_bno, 1);
        brelse(bh_new);
        brelse(bh_old);
    } else {
        printk("The new block is not empty. The current block on the wanted block, will be moved somewhere else and the old block will be removed\n");
        tmp_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, sbi->s_unused_area, 1);

        bh_old = sb_bread(sb, old_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, *offset);

        if (!bh_new) {
            brelse(bh_old);
            return -EIO;
        }

        bh_swap = sb_bread(sb, tmp_bno);

        if (!bh_swap) {
            brelse(bh_old);
            brelse(bh_swap);
            return -EIO;
        }
        disk_block_old = (struct basicbtfs_disk_block *) bh_old->b_data;
        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
        disk_block_swap = (struct basicbtfs_disk_block *) bh_swap->b_data;

        memcpy(disk_block_swap, disk_block_new, BASICBTFS_BLOCKSIZE);
        memcpy(disk_block_new, disk_block_old, BASICBTFS_BLOCKSIZE);
        basicbtfs_defrag_move_btree_node(sb, bh_new, old_bno, *offset);


        switch (disk_block_swap->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                basicbtfs_defrag_move_btree_node(sb, bh_swap, *offset, tmp_bno);
                printk("need to move btree\n");
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
            printk("need to move cluster table\n");
                basicbtfs_defrag_move_cluster_table(sb, bh_swap, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                printk("need to move namelist\n");
                basicbtfs_defrag_move_namelist(sb, bh_swap, tmp_bno);
                break;
            default:
                printk("need to move file block\n");
                basicbtfs_defrag_move_file_block(sb, bh_swap, tmp_bno);
                break;
        }

        put_blocks(sbi, old_bno, 1);
        mark_buffer_dirty(bh_swap);
        mark_buffer_dirty(bh_new);
        brelse(bh_swap);
        brelse(bh_old);
        brelse(bh_new);
    }
    *offset += 1;
    printk("New offset after updating a btree node %d\n", *offset);
    return 0;
}

static inline int basicbtfs_defrag_btree(struct super_block *sb, struct inode *inode, uint32_t bno, uint32_t *offset) {
    /**
     * 1. If root, move btree and update inode->i_bno on disk and cache
     * 2. if non-root, move btree and update parent reference
     */

    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int index = 0, ret = 0;

    basicbtfs_defrag_btree_node(sb, bno, offset);

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    // node = (struct basicbtfs_btree_node *) bh->b_data;
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_defrag_btree(sb, inode, node->children[index], offset);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }

        // printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_defrag_btree(sb, inode, node->children[index], offset);
    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_defrag_move_btree_node(struct super_block *sb, struct buffer_head *bh, uint32_t cur_bno, uint32_t new_bno) {
    struct buffer_head *bh_parent, *bh_child;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_parent, *disk_child;
    struct basicbtfs_btree_node *btr_node = NULL, *btr_node_parent, *btr_node_child;
    struct inode *parent_inode;
    int index = 0;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    btr_node = &disk_block->block_type.btree_node;

    if (btr_node->root) {
        parent_inode = basicbtfs_iget(sb, btr_node->parent);
        basicbtfs_btree_update_root(parent_inode, new_bno);

    } else {
        bh_parent = sb_bread(sb, btr_node->parent);
        if (!bh_parent) return -EIO;

        disk_parent = (struct basicbtfs_disk_block *) bh_parent->b_data;
        btr_node_parent = &disk_parent->block_type.btree_node;

        for (index = 0; index <= btr_node_parent->nr_of_keys; index++) {
            if (btr_node_parent->children[index] == cur_bno) {
                btr_node_parent->children[index] = new_bno;
                break;
            }
        }
        mark_buffer_dirty(bh_parent);
        brelse(bh_parent);
    }

    for (index = 0; index <= btr_node->nr_of_keys; index++) {
        bh_child = sb_bread(sb, btr_node->children[index]);

        if (!bh_child) return -EIO;

        disk_child = (struct basicbtfs_disk_block *) bh_child->b_data;
        btr_node_child = &disk_child->block_type.btree_node;
        btr_node_child->parent = new_bno;
        mark_buffer_dirty(bh_child);
        brelse(bh_child);
    }
    return 0;
}

static inline int basicbtfs_defrag_move_namelist(struct super_block *sb, struct buffer_head *bh,  uint32_t new_bno) {
    struct buffer_head *bh_prev = NULL, *bh_next;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL, *name_list_prev, *name_list_next;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_block_prev, *disk_block_next;
    struct basicbtfs_btree_node *btr_node = NULL;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;

    if (name_list_hdr->first_list) {
        bh_prev = sb_bread(sb, name_list_hdr->prev_block);

        if (!bh_prev) return -EIO;

        disk_block_prev = (struct basicbtfs_disk_block *) bh_prev->b_data;
        btr_node = &disk_block_prev->block_type.btree_node;

        btr_node->tree_name_bno = new_bno;

        mark_buffer_dirty(bh_prev);
        brelse(bh_prev);
    } else {
        bh_prev = sb_bread(sb, name_list_hdr->prev_block);

        if (!bh_prev) return -EIO;

        disk_block_prev = (struct basicbtfs_disk_block *) bh_prev->b_data;
        name_list_prev = &disk_block_prev->block_type.name_list_hdr;

        name_list_prev->next_block = new_bno;
        mark_buffer_dirty(bh_prev);
        brelse(bh_prev);
    }

    if (name_list_hdr->next_block != 0) {
        bh_next = sb_bread(sb, name_list_hdr->next_block);

        if (!bh_next) return -EIO;

        disk_block_next = (struct basicbtfs_disk_block *) bh_next->b_data;
        name_list_next = &disk_block_next->block_type.name_list_hdr;

        name_list_next->prev_block = new_bno;
        mark_buffer_dirty(bh_next);
        brelse(bh_next);
    }

    return 0;
}

static inline void basicbtfs_defrag_move_cluster_table(struct super_block *sb, struct buffer_head *bh, uint32_t new_bno) {
    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block;
    struct inode *inode = NULL;


    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    cluster_list = &disk_block->block_type.cluster_table;
    inode = basicbtfs_iget(sb, cluster_list->ino);
    if (!inode) {
        printk("ino did exist: %d\n", cluster_list->ino);
    } else {
        printk("moved bno: %d\n", BASICBTFS_INODE(inode)->i_bno);
    }
    basicbtfs_file_update_root(inode, new_bno);
}

static inline int basicbtfs_defrag_move_file_block(struct super_block *sb, struct buffer_head *bh_old, uint32_t new_bno) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct inode *inode;
    struct basicbtfs_inode_info *inode_info = NULL;
    uint32_t ino = 0, cluster_index, tmp_bno;
    struct buffer_head *bh = NULL, *bh_new, *bh_old2;
    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block, *disk_block_new, *disk_block_old;
    int i = 0;


    ino = sbi->s_fileblock_map[new_bno].ino;
    cluster_index = sbi->s_fileblock_map[new_bno].cluster_index;

    inode = basicbtfs_iget(sb, ino);
    inode_info = BASICBTFS_INODE(inode);

    bh = sb_bread(sb, inode_info->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    cluster_list = &disk_block->block_type.cluster_table;

    tmp_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, sbi->s_unused_area, cluster_list->table[cluster_index].cluster_length);

    bh_new = sb_bread(sb, tmp_bno);

    if (!bh_new) return -EIO;


    disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
    disk_block_old = (struct basicbtfs_disk_block *) bh_old->b_data;
    memcpy(disk_block_new, disk_block_old, BASICBTFS_BLOCKSIZE);

    brelse(bh_new);

    for (i = 1; i < cluster_list->table[cluster_index].cluster_length; i++) {
        bh_old2 = sb_bread(sb, cluster_list->table[cluster_index].start_bno + i);

        if (!bh_old2) return -EIO;

        bh_new = sb_bread(sb, tmp_bno + i);
        
        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
        disk_block_old = (struct basicbtfs_disk_block *) bh_old2->b_data;
        memcpy(disk_block_new, disk_block_old, BASICBTFS_BLOCKSIZE);

        mark_buffer_dirty(bh_new);
        brelse(bh_old2);
        brelse(bh_new);
    }

    put_blocks(sbi, cluster_list->table[cluster_index].start_bno + 1, cluster_list->table[cluster_index].cluster_length - 1);
    put_blocks(sbi, new_bno, 1);
    cluster_list->table[cluster_index].start_bno = tmp_bno;

    return 0;
}

static inline int basicbtfs_defrag_nametree_block(struct super_block *sb, struct inode *inode, struct buffer_head *bh, uint32_t name_bno, uint32_t *offset) {
    /**
     * 1. First fill up all open places and update corresponding entry in btree and update
     * 2. Use next block to fill up current block and update corresponding entries in btree
     * 3. Update corresponding block in cache and delete next block if no entries there
     * 4. swap to wanted to place. Check block type and move the block which occupies the spot to the unused area. update references
     * 4. end
     */
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL, *name_list_hdr_next;
    struct basicbtfs_name_entry *cur_entry = NULL, *cur_entry_next;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_block_next, *disk_block_new, *disk_block_swap;
    struct buffer_head *bh_next = NULL, *bh_new, *bh_swap;
    char *block = NULL, *block_next = NULL;
    uint32_t pos = 0, pos_next = 0, rest_of_block = 0;
    uint32_t entries_to_move = 0, tmp_bno, new_bno;
    char *filename = NULL;
    int i = 0;

    printk("defragment namelist block and offset: %d | %d\n", name_bno, *offset);


    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    block = (char *) bh->b_data;
    block += BASICBTFS_NAME_ENTRY_S_OFFSET;
    pos += BASICBTFS_NAME_ENTRY_S_OFFSET;
    cur_entry = (struct basicbtfs_name_entry *) block;

    printk("start iterating name list\n");

    for (i = 0; i < name_list_hdr->nr_of_entries; i++) {
        block += sizeof(struct basicbtfs_name_entry);
        pos += sizeof(struct basicbtfs_name_entry);
        if (cur_entry->ino != 0) {
            filename = (char *)kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash_from_block(filename, cur_entry->name_length), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
            kfree(filename);
        } else {
            rest_of_block = BASICBTFS_BLOCKSIZE - (pos + cur_entry->name_length);
            name_list_hdr->start_unused_area -= (sizeof(struct basicbtfs_name_entry) + cur_entry->name_length);
            memcpy(block - sizeof(struct basicbtfs_name_entry) , block + cur_entry->name_length, rest_of_block);
            i--;
            continue;
        }

        block += cur_entry->name_length;
        pos += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
    }

    printk("final pos: %d\n", pos);
    rest_of_block = BASICBTFS_BLOCKSIZE - pos;

    if (name_list_hdr->next_block != 0) {
        bh_next = sb_bread(sb, name_list_hdr->next_block);

        if (!bh_next) {
            brelse(bh);
            return -EIO;
        }

        disk_block_next = (struct basicbtfs_disk_block *) bh_next->b_data;
        name_list_hdr_next = &disk_block_next->block_type.name_list_hdr;
        // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
        block_next = (char *) bh_next->b_data;
        block_next += BASICBTFS_NAME_ENTRY_S_OFFSET;
        pos_next += BASICBTFS_NAME_ENTRY_S_OFFSET;
        cur_entry_next = (struct basicbtfs_name_entry *) block_next;

        for (i = 0; i < name_list_hdr_next->nr_of_entries; i++) {
            block_next += sizeof(struct basicbtfs_name_entry);
            pos_next += sizeof(struct basicbtfs_name_entry);
            if ((BASICBTFS_BLOCKSIZE - name_list_hdr->start_unused_area) >  cur_entry_next->name_length +  sizeof(struct basicbtfs_name_entry)) {
                if (cur_entry_next->ino != 0) {
                    memcpy(block, block_next - sizeof(struct basicbtfs_name_entry), cur_entry_next->name_length +  sizeof(struct basicbtfs_name_entry));
                    entries_to_move++;
                    cur_entry_next->ino = 0;
                    rest_of_block -= (cur_entry_next->name_length +  sizeof(struct basicbtfs_name_entry));
                    name_list_hdr->free_bytes -= (sizeof(struct basicbtfs_name_entry) + cur_entry_next->name_length);
                    name_list_hdr->start_unused_area += (sizeof(struct basicbtfs_name_entry) + cur_entry_next->name_length);
                    name_list_hdr_next->free_bytes += (sizeof(struct basicbtfs_name_entry) + cur_entry_next->name_length);

                    filename = (char *)kzalloc(sizeof(char) * cur_entry_next->name_length, GFP_KERNEL);
                    strncpy(filename, block_next, cur_entry_next->name_length);
                    basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash_from_block(filename, cur_entry_next->name_length), 0, name_bno, pos_next - sizeof(struct basicbtfs_name_entry));
                    kfree(filename);
                } else {
                    i--;
                }

                // filename_next = kzalloc(sizeof(char) * cur_entry_next->name_length, GFP_KERNEL);
                // strncpy(filename_next, block_next, cur_entry_next->name_length);

                // kfree(filename_next);
            } else {
                break;
            }

            block += cur_entry->name_length;
            pos += cur_entry->name_length;
            cur_entry = (struct basicbtfs_name_entry *) block;
        }

        name_list_hdr_next->nr_of_entries -= entries_to_move;
        name_list_hdr->nr_of_entries += entries_to_move;
    }
    

    // check if offset is empty, then take spot and copy item, otherwise 
    if (*offset == name_bno) {
        printk("offset and current block are the same. Nothing  needs to be changed\n");
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1)) {
        printk("offset is free. Take the offset block and delete old block\n");
        new_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1);

        bh_new = sb_bread(sb, new_bno);

        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
        memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);
        
        basicbtfs_defrag_move_namelist(sb, bh_new, new_bno);

        mark_buffer_dirty(bh_new);
        brelse(bh_new);
        put_blocks(sbi, name_bno, 1);
    } else {
        printk("offset is not free. Find new place for the wanted block and delete old block\n");
        // swap
        tmp_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, sbi->s_unused_area, 1);

        bh_swap = sb_bread(sb, tmp_bno);
        if (!bh_swap) {
            return -EIO;
        }

        bh_new = sb_bread(sb, *offset);

        if (!bh_new) {
            return -EIO;
        }

        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
        disk_block_swap = (struct basicbtfs_disk_block *) bh_swap->b_data;

        memcpy(disk_block_swap, disk_block_new, BASICBTFS_BLOCKSIZE);
        memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);
        basicbtfs_defrag_move_namelist(sb, bh_new, *offset);

        switch (disk_block_swap->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                printk("need to move btree\n");
                basicbtfs_defrag_move_btree_node(sb, bh_swap, *offset, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                printk("need to move cluster table\n");
                basicbtfs_defrag_move_cluster_table(sb, bh_swap, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                printk("need to move nametree\n");
                basicbtfs_defrag_move_namelist(sb, bh_swap, tmp_bno);
                break;
            default:
                printk("need to move file block\n");
                basicbtfs_defrag_move_file_block(sb, bh_swap, tmp_bno);
                break;
        }

        put_blocks(sbi, name_bno, 1);
    }

    disk_block = (struct basicbtfs_disk_block *) bh->b_data; // to be updated
    name_list_hdr = &disk_block->block_type.name_list_hdr;
    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
    block = (char *) bh->b_data;
    block += BASICBTFS_NAME_ENTRY_S_OFFSET;
    pos += BASICBTFS_NAME_ENTRY_S_OFFSET;
    cur_entry = (struct basicbtfs_name_entry *) block;

    for (i = 0; i < name_list_hdr->nr_of_entries; i++) {
        block += sizeof(struct basicbtfs_name_entry);
        pos += sizeof(struct basicbtfs_name_entry);
        if (cur_entry->ino != 0) {
            filename = (char *)kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash_from_block(filename, cur_entry->name_length), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
            kfree(filename);
        } else {
            i--;
        }

        block += cur_entry->name_length;
        pos += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
    }
    *offset += 1;
    printk("offset after defragmenting a namelist block: %d\n", *offset);
    return 0;
}

static inline int basicbtfs_defrag_nametree(struct super_block *sb, struct inode *inode, uint32_t *offset) {
    /**
     * 1. while to defrag until all blocks have been traversed
     * 
     */
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct basicbtfs_disk_block *disk_block;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct buffer_head *bh = NULL;
    uint32_t cur_namelist_bno = 0;
    printk("defragment namelist\n");

    bh = sb_bread(sb, inode_info->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;
    cur_namelist_bno = node->tree_name_bno;

    brelse(bh);

    bh = sb_bread(sb, cur_namelist_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    name_list_hdr = &disk_block->block_type.name_list_hdr;

    basicbtfs_defrag_nametree_block(sb, inode, bh, cur_namelist_bno, offset);

    while (name_list_hdr->next_block != 0) {
        cur_namelist_bno = name_list_hdr->next_block;
        brelse(bh);

        bh = sb_bread(sb, cur_namelist_bno);

        if (!bh) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;

        basicbtfs_defrag_nametree_block(sb, inode, bh, cur_namelist_bno, offset);
    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_defrag_file_table_block(struct super_block *sb, struct inode *inode, uint32_t *offset) {
    /**
     * 1. defrag filetable block and place on wanted place. If necessary, move current block based on the block filetype.
     * 2. Iterate over all file extents and swap the files if necessary.
     * end
     */

    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_disk_block *disk_block, *disk_block_new, *disk_block_swap;
    struct buffer_head *bh_old = NULL, *bh_new = NULL, *bh_swap, *bh_old_block, *bh_new_block, *bh_swap_block;
    uint32_t cluster_index = 0, block_index = 0, disk_block_offset = 0, tmp_bno, new_bno, new_start_bno;
    printk("start defragging of file : %d | %d\n", *offset, BASICBTFS_INODE(inode)->i_bno);
    if (*offset == BASICBTFS_INODE(inode)->i_bno) {
        printk("offset is same as current block");
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1)) {
        printk("offset is free. Take it and remove old block\n");
        new_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1);
        printk("new_bno: %d\n", new_bno);
        bh_old = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, new_bno);

        if (!bh_new) return -EIO;
        
        disk_block = (struct basicbtfs_disk_block *) bh_old->b_data;
        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;

        memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);
        put_blocks(sbi, BASICBTFS_INODE(inode)->i_bno, 1);
        basicbtfs_file_update_root(inode, *offset);

        mark_buffer_dirty(bh_new);
        brelse(bh_old);
        brelse(bh_new);
    } else {
        printk("offset is not free. swap it and remove old block: %d\n", sbi->s_unused_area);
        tmp_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, sbi->s_unused_area, 1);
        printk("new tmp_bno: %d\n", tmp_bno);

        bh_swap = sb_bread(sb, tmp_bno);

        if (!bh_swap) return -EIO;

        bh_old = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, *offset);

        if (!bh_new) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh_old->b_data;
        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;
        disk_block_swap = (struct basicbtfs_disk_block *) bh_swap->b_data;

        memcpy(disk_block_swap, disk_block_new, BASICBTFS_BLOCKSIZE);
        memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);
        
        switch (disk_block_swap->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                basicbtfs_defrag_move_btree_node(sb, bh_swap, *offset, tmp_bno);
                printk("updated btree on wanted place\n");
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                basicbtfs_defrag_move_cluster_table(sb, bh_swap, tmp_bno);
                printk("updated cluster table on wanted place\n");
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                basicbtfs_defrag_move_namelist(sb, bh_swap, tmp_bno);
                printk("updated nametree on wanted place\n");
                break;
            default:
                basicbtfs_defrag_move_file_block(sb, bh_swap, tmp_bno);
                printk("updated file block on wanted place\n");
                break;
        }
        // printk("switch case is done\n");
        put_blocks(sbi, BASICBTFS_INODE(inode)->i_bno, 1);
        // printk("block has been freed\n");
        mark_buffer_dirty(bh_swap);
        mark_buffer_dirty(bh_new);
        brelse(bh_swap);
        brelse(bh_old);
        brelse(bh_new);
        basicbtfs_file_update_root(inode, *offset);
        // printk("cluster has been udpated\n");
    }

    printk("cluster table is updated\n");
    bh_new = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);
    disk_block = (struct basicbtfs_disk_block *) bh_new->b_data;
    cluster_list = &disk_block->block_type.cluster_table;

    *offset += 1;

    for (cluster_index = 0; cluster_index < BASICBTFS_ATABLE_MAX_CLUSTERS; cluster_index++) {
        new_start_bno = *offset;

        if (cluster_list->table[cluster_index].start_bno == 0) {
            printk("end of cluster table with index: %d\n", cluster_index);
            break;
        }
        for (block_index = 0; block_index < cluster_list->table[cluster_index].cluster_length; block_index++) {
            disk_block_offset = cluster_list->table[cluster_index].start_bno;

            // if is empty take
            // else if-not empty swap
            printk("index, bno, and length: %d | %d | %d\n", cluster_index, disk_block_offset, cluster_list->table[cluster_index].cluster_length);
            printk("offset and current block: %d | %d\n", *offset, disk_block_offset + block_index);
            if (*offset == disk_block_offset + block_index) {
                printk("same offset\n");
                *offset += 1;
                continue;
            } else if (sbi && is_bit_empty(sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, block_index)) {
                printk("wanted offset is free\n");
                // if (block_index > 0) continue;
                new_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, *offset, 1);

                printk("new bno: %d\n", new_bno);
                // if (block_index > 2) {
                //     *offset += 1;
                //     continue;
                // }

                bh_new_block = sb_bread(sb, new_bno);

                if (!bh_new_block) return -EIO;

                bh_old_block = sb_bread(sb, disk_block_offset + block_index);

                if (!bh_old_block) return -EIO;

                disk_block = (struct basicbtfs_disk_block *) bh_old_block->b_data;
                disk_block_new = (struct basicbtfs_disk_block *) bh_new_block->b_data;

                memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);

                put_blocks(sbi, disk_block_offset + block_index, 1);
                mark_buffer_dirty(bh_new_block);
                // mark_buffer_dirty(bh_old_block);
                brelse(bh_new_block);
                brelse(bh_old_block);
            } else {
                printk("wanted offset should be swept\n");
                // swap
                uint32_t tmp_bno = get_offset(sbi, sbi->s_bfree_bitmap, sbi->s_nblocks, sbi->s_unused_area, 1);

                bh_swap_block = sb_bread(sb, tmp_bno);
                if (!bh_swap_block) {
                    return -EIO;
                }

                bh_new_block = sb_bread(sb, *offset);

                if (!bh_new_block) {
                    return -EIO;
                }

                bh_old_block = sb_bread(sb, cluster_list->table[cluster_index].start_bno + block_index);

                if (!bh_old_block) {
                    return -EIO;
                }

                disk_block = (struct basicbtfs_disk_block *) bh_old_block->b_data;
                disk_block_new = (struct basicbtfs_disk_block *) bh_new_block->b_data;
                disk_block_swap = (struct basicbtfs_disk_block *) bh_swap_block->b_data;

                memcpy(disk_block_swap, disk_block_new, BASICBTFS_BLOCKSIZE);
                memcpy(disk_block_new, disk_block, BASICBTFS_BLOCKSIZE);

                switch (disk_block_swap->block_type_id) {
                    case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                        printk("need to move btree\n");
                        basicbtfs_defrag_move_btree_node(sb, bh_swap_block, *offset, tmp_bno);
                        break;
                    case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                        printk("need to move cluster table\n");
                        basicbtfs_defrag_move_cluster_table(sb, bh_swap_block, tmp_bno);
                        break;
                    case BASICBTFS_BLOCKTYPE_NAMETREE:
                        printk("need to move nametree\n");
                        basicbtfs_defrag_move_namelist(sb, bh_swap_block, tmp_bno);
                        break;
                    default:
                        printk("need to move file block\n");
                        basicbtfs_defrag_move_file_block(sb, bh_swap_block, tmp_bno);
                        break;
                }

                put_blocks(sbi, cluster_list->table[cluster_index].start_bno + block_index, 1);
                mark_buffer_dirty(bh_swap_block);
                mark_buffer_dirty(bh_new_block);
                brelse(bh_swap_block);
                brelse(bh_old_block);
                brelse(bh_new_block);
            }
            *offset += 1;
        }
        cluster_list->table[cluster_index].start_bno = new_start_bno;
        printk("it has been updated: %d\n", cluster_list->table[cluster_index].start_bno);

    }

    end:
    mark_buffer_dirty(bh_new);
    brelse(bh_new);
    return 0;
}

static inline int basicbtfs_defrag_traverse_directory(struct super_block *sb, uint32_t bno, uint32_t *offset) {
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct inode *inode = NULL;
    int index = 0, ret = 0;

    bh = sb_bread(sb, bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    for (index = 0; index < node->nr_of_keys; index++) {
        if (!node->leaf) {
            ret = basicbtfs_defrag_traverse_directory(sb, node->children[index], offset);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }

        inode = basicbtfs_iget(sb, node->entries[index].ino);
        if (S_ISDIR(inode->i_mode)) {
            printk("directory to be defragmented\n");
            basicbtfs_defrag_directory(sb, inode, offset);
        } else if (S_ISREG(inode->i_mode)) {
            printk("will defrag file\n");
            basicbtfs_defrag_file_table_block(sb, inode, offset);
            printk("offset after defrag file: %d\n", *offset);
        }


        printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_defrag_traverse_directory(sb, node->children[index], offset);
    }

    brelse(bh);
    return 0;
}

static inline void basicbtfs_defrag_directory(struct super_block *sb, struct inode *inode, uint32_t *offset) {
    /**
     * 1. Defrag nametree
     * 2. Defrag btree
     * 3. iterate through all files and defrag based on filetype
     * 4. If directory, defrag directory add to tmp list and defrag after all files have been defragged
     * 5. if file, defrag file
     * 6. defrag directories on list
     * end
     */

    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    printk("defrag directory before: %d\n", *offset);
    basicbtfs_defrag_btree(sb, inode, inode_info->i_bno, offset);
    // printk("defragmented btree with offset: %d\n", *offset);
    // // basicbtfs_btree_traverse_debug(sb, inode_info->i_bno);
    basicbtfs_defrag_nametree(sb, inode, offset);
    // printk("defragmented nametree with offset: %d\n", *offset);
    basicbtfs_defrag_traverse_directory(sb, inode_info->i_bno, offset);
    printk("defrag directory after: %d\n", *offset);

}

static inline int basicbtfs_defrag_disk(struct super_block *sb, struct inode *inode) {
    /**
     * 1. 
     * 3. Defrag directory
     */
    struct basicbtfs_sb_info *sbi = (struct basicbtfs_sb_info *)BASICBTFS_SB(sb);
    uint32_t offset = BASICBTFS_INODE(inode)->i_bno;
    printk("unused area: %d | %d\n", sbi->s_unused_area, offset);
    basicbtfs_defrag_directory(sb, inode, &offset);
    sbi->s_unused_area = offset;
    printk("unused area: %d\n", sbi->s_unused_area);
    return 0;
}

#endif 
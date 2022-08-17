#ifndef BASICBTFS_DEFRAG_H
#define BASICBTFS_DEFRAG_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>

#include "basicbtfs.h"
#include "btree.h"
#include "bitmap.h"
#include "cache.h"
#include "init.h"

static inline int basicbtfs_defrag_btree_node(struct super_block *sb, uint32_t old_bno, uint32_t *offset) {
    struct buffer_head *bh_old = NULL, *bh_new, *bh_swap;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_disk_block *disk_block_new = NULL, *disk_block_old, *disk_block_swap;
    int index = 0, ret = 0;

    if (old_bno == *offset) {
        printk("same\n");
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset, 1)) {
        uint32_t new_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset, 1);

        bh_old = sb_bread(sb, old_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, new_bno);

        if (!bh_new) return -EIO;

        memcpy(bh_new->b_data, bh_old->b_data, BASICBTFS_BLOCKSIZE);
        mark_buffer_dirty(bh_new);
        brelse(bh_new);
        brelse(bh_old);
    } else {
        uint32_t tmp_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_unused_area, 1);

        bh_old = sb_bread(sb, old_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, *offset);

        if (!bh_new) return -EIO;

        bh_swap = sb_bread(sb, tmp_bno);

        if (!bh_swap) return -EIO;

        memcpy(bh_swap->b_data, bh_new->b_data, BASICBTFS_BLOCKSIZE);
        memcpy(bh_new->b_data, bh_old->b_data, BASICBTFS_BLOCKSIZE);

        disk_block_swap = (struct basicbtfs_disk_block *) bh_swap->b_data;

        switch (disk_block_swap->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                basicbtfs_defrag_move_btree_node(sb, bh_swap, *offset, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                basicbtfs_defrag_move_cluster_table(sb, bh_swap, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                basicbtfs_defrag_move_namelist(sb, bh_swap, tmp_bno);
                break;
            default:
                basicbtfs_defrag_move_file_block(sb, bh_swap, tmp_bno);
                break;
        }

        put_blocks(sbi, old_bno, 1);
        mark_buffer_dirty(bh_swap);
        mark_buffer_dirty(bh_old);
        mark_buffer_dirty(bh_new);
        brelse(bh_swap);
        brelse(bh_old);
        brelse(bh_new);
    }

    *offset++;
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

        printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_defrag_btree(sb, inode, node->children[index], offset);
    }

    brelse(bh);
    return 0;
}

static inline int basicbtfs_defrag_move_btree_node(struct super_block *sb, struct buffer_head *bh, uint32_t cur_bno, uint32_t new_bno) {
    struct buffer_head *bh_parent;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_parent;
    struct basicbtfs_btree_node *btr_node = NULL, *btr_node_parent;
    struct inode *parent_inode;
    uint32_t ret = 0, child = 0;
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
                btr_node_parent->parent = new_bno;
                break;
            }
        }

    }
    return 0;
}

static inline int basicbtfs_defrag_move_namelist(struct super_block *sb, struct buffer_head *bh,  uint32_t new_bno) {
    struct buffer_head *bh_prev = NULL, *bh_next;
    char *block = NULL;
    struct basicbtfs_name_entry *name_entry = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL, *name_list_prev, *name_list_next;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_block_prev, *disk_block_next;
    struct basicbtfs_btree_node *btr_node = NULL;
    uint32_t old_free_bytes = 0;

    // name_list_hdr = (struct basicbtfs_name_list_hdr *) bh->b_data;
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

        name_list_next->next_block = new_bno;
        mark_buffer_dirty(bh_next);
        brelse(bh_next);
    }

    return 0;
}

static inline void basicbtfs_defrag_move_cluster_table(struct super_block *sb, struct buffer_head *bh, uint32_t new_bno) {
    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block;
    int ret = 0, bno;
    struct inode *inode = NULL;
    uint32_t cluster_index = 0;


    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    cluster_list = &disk_block->block_type.cluster_table;
    inode = basicbtfs_iget(sb, cluster_list->ino);

    basicbtfs_btree_update_root(inode, new_bno);
}

static inline int basicbtfs_defrag_move_file_block(struct super_block *sb, struct buffer_head *bh_old, uint32_t new_bno) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct inode *inode;
    struct basicbtfs_inode_info *inode_info = NULL;
    uint32_t ino = 0, block, cluster_index;
    struct buffer_head *bh = NULL, *bh_new, *bh_old;
    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block;
    int i = 0;


    ino = sbi->s_fileblock_map[new_bno].ino;
    cluster_index = sbi->s_fileblock_map[new_bno].cluster_index;

    inode = basicbtfs_iget(sb, ino);
    inode_info = BASICBTFS_INODE(inode);

    bh = sb_bread(sb, inode_info->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    cluster_list = &disk_block->block_type.cluster_table;

    uint32_t tmp_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_unused_area, cluster_list->table[cluster_index].cluster_length);

    bh_new = sb_bread(sb, tmp_bno);

    if (!bh_new) return -EIO;

    memcpy(bh_new, bh_old, BASICBTFS_BLOCKSIZE);

    brelse(bh_new);

    for (i = 1; i < cluster_list->table[cluster_index].cluster_length; i++) {
        bh_old = sb_bread(sb, cluster_list->table[cluster_index].start_bno + i);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, tmp_bno + i);

        memcpy(bh_new, bh_old, BASICBTFS_BLOCKSIZE);

        mark_buffer_dirty(bh_old);
        mark_buffer_dirty(bh_new);
        brelse(bh_old);
        brelse(bh_new);
    }

    cluster_list->table[cluster_index].start_bno = tmp_bno;
    put_blocks(sbi, cluster_list->table[cluster_index].start_bno, cluster_list->table[cluster_index].cluster_length - 1);
    put_blocks(sbi, new_bno, 1);

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
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL, *name_list_hdr_next, *name_list_hdr_prev;
    struct basicbtfs_name_entry *cur_entry = NULL, *cur_entry_next;
    struct basicbtfs_disk_block *disk_block = NULL, *disk_block_next, *disk_block_prev, *disk_block_new;
    struct buffer_head *bh_next = NULL, *bh_new, *bh_prev, *bh_wanted;
    char *block = NULL, *block_next = NULL;
    uint32_t pos = 0, pos_next = 0, rest_of_block = 0;
    uint32_t entries_to_move = 0;
    char *filename = NULL, *filename_next;
    int i = 0;

    
    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
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
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash_from_block(filename, cur_entry_next->name_length), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
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

    // check if offset is empty, then take spot and copy item, otherwise 
    if (*offset == name_bno) {
        return;
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset, 1)) {
        uint32_t new_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset, 1);

        bh_new = sb_bread(sb, new_bno);
        memcpy(bh_new->b_data, bh->b_data, BASICBTFS_BLOCKSIZE);

        name_list_hdr_next->prev_block = new_bno;

        bh_prev = sb_bread(sb, name_list_hdr->prev_block);

        if (!bh_prev) {
            brelse(bh);
            brelse(bh_new);
            return -EIO;
        }

        disk_block_prev = (struct basicbtfs_disk_block *) bh_prev->b_data;
        name_list_hdr_prev = &disk_block_prev->block_type.name_list_hdr;
        
        name_list_hdr_prev->next_block = new_bno;

        mark_buffer_dirty(bh_new);
        brelse(bh_new);
        put_blocks(sbi, name_bno, 1);
    } else {
        // swap
        uint32_t tmp_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_unused_area, 1);

        bh_new = sb_bread(sb, tmp_bno);
        if (!bh_new) {
            return -EIO;
        }

        bh_wanted = sb_bread(sb, *offset);

        if (!bh_wanted) {

        }

        memcpy(bh_new->b_data, bh_wanted->b_data, BASICBTFS_BLOCKSIZE);
        memcpy(bh_wanted->b_data, bh->b_data, BASICBTFS_BLOCKSIZE);
        name_list_hdr_next->prev_block = *offset;
        
        bh_prev = sb_bread(sb, name_list_hdr->prev_block);

        if (!bh_prev) {
            brelse(bh);
            brelse(bh_new);
            return -EIO;
        }

        disk_block_prev = (struct basicbtfs_disk_block *) bh_prev->b_data;
        name_list_hdr_prev = &disk_block_prev->block_type.name_list_hdr;
        name_list_hdr_prev->next_block = *offset;

        disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;

        switch (disk_block_new->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                basicbtfs_defrag_move_btree_node(sb, bh_new, *offset, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                basicbtfs_defrag_move_cluster_table(sb, bh_new, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                basicbtfs_defrag_move_namelist(sb, bh_new, tmp_bno);
                break;
            default:
                basicbtfs_defrag_move_file_block(sb, bh_new, tmp_bno);
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
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash_from_block(filename, cur_entry_next->name_length), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
            kfree(filename);
        } else {
            i--;
        }

        block += cur_entry->name_length;
        pos += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
    }

    *offset++;
    return 0;
}

static inline int basicbtfs_defrag_nametree(struct super_block *sb, inode *inode, uint32_t *offset) {
    /**
     * 1. while to defrag until all blocks have been traversed
     * 
     */
    struct basicbtfs_inode_info *inode_info = BASICBTFS_INODE(inode);
    struct basicbtfs_disk_block *disk_block;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct buffer_head *bh = NULL, *bh_name;
    uint32_t cur_namelist_bno = 0;

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

static inline int basicbtfs_defrag_file_table_block(struct super_block *sb, inode *inode, uint32_t *offset) {
    /**
     * 1. defrag filetable block and place on wanted place. If necessary, move current block based on the block filetype.
     * 2. Iterate over all file extents and swap the files if necessary.
     * end
     */

    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_sb_info *sbi;
    struct basicbtfs_disk_block *disk_block, *disk_block_new;
    struct buffer_head *bh_old = NULL, *bh_new = NULL, *bh_swap, *bh_old_block, *bh_new_block, *bh_swap_block;
    int ret = 0, bno;
    uint32_t cluster_index = 0, block_index = 0, disk_block_offset = 0;

    if (*offset == BASICBTFS_INODE(inode)->i_bno) {

    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset, 1)) {
        bh_old = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, *offset);

        memcpy(bh_new, bh_old, BASICBTFS_BLOCKSIZE);
        put_blocks(sbi, BASICBTFS_INODE(inode)->i_bno, 1);
        basicbtfs_btree_update_root(inode, *offset);

        disk_block = (struct basicbtfs_disk_block *) bh_new->b_data;
        cluster_list = &disk_block->block_type.cluster_table;
    } else {
        uint32_t tmp_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_unused_area, 1);

        bh_swap = sb_bread(sb, tmp_bno);

        if (!bh_swap) return -EIO;

        bh_old = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);

        if (!bh_old) return -EIO;

        bh_new = sb_bread(sb, *offset);

        if (!bh_new) return -EIO;

        memcpy(bh_swap->b_data, bh_new->b_data, BASICBTFS_BLOCKSIZE);
        memcpy(bh_new->b_data, bh_old->b_data, BASICBTFS_BLOCKSIZE);


        disk_block_new = (struct basicbtfs_disk_block *) bh_swap->b_data;

        switch (disk_block_new->block_type_id) {
            case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                basicbtfs_defrag_move_btree_node(sb, bh_swap, *offset, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                basicbtfs_defrag_move_cluster_table(sb, bh_swap, tmp_bno);
                break;
            case BASICBTFS_BLOCKTYPE_NAMETREE:
                basicbtfs_defrag_move_namelist(sb, bh_swap, tmp_bno);
                break;
            default:
                basicbtfs_defrag_move_file_block(sb, bh_swap, tmp_bno);
                break;
        }

        put_blocks(sbi, BASICBTFS_INODE(inode)->i_bno, 1);
        mark_buffer_dirty(bh_swap_block);
        mark_buffer_dirty(bh_old_block);
        mark_buffer_dirty(bh_new_block);
        basicbtfs_btree_update_root(inode, *offset);
        brelse(bh_swap_block);
        brelse(bh_old_block);
        brelse(bh_new_block);
    }

    *offset++;

    for (cluster_index = 0; cluster_index < BASICBTFS_ATABLE_MAX_CLUSTERS; cluster_index++) {
        for (block_index = 0; block_index < cluster_list->table[cluster_index].cluster_length; block_index++) {
            disk_block_offset = cluster_list->table[cluster_index].start_bno;
            // if is empty take
            // else if-not empty swap
             if (*offset == disk_block_offset + block_index) {
                return;
            } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset + block_index, 1)) {
                uint32_t new_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, *offset + block_index, 1);

                bh_new_block = sb_bread(sb, new_bno);

                bh_old_block = sb_bread(sb, cluster_list->table[cluster_index].start_bno + block_index);
                memcpy(bh_new_block->b_data, bh_old_block->b_data, BASICBTFS_BLOCKSIZE);

                put_blocks(sbi, cluster_list->table[cluster_index].start_bno + block_index, 1);
                mark_buffer_dirty(bh_new_block);
                mark_buffer_dirty(bh_old_block);
                brelse(bh_new_block);
                brelse(bh_old_block);
            } else {
                // swap
                uint32_t tmp_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_unused_area, 1);

                bh_swap_block = sb_bread(sb, tmp_bno);
                if (!bh_swap_block) {
                    return -EIO;
                }

                bh_new_block = sb_bread(sb, *offset + block_index);

                if (!bh_new_block) {
                    return -EIO;
                }

                bh_old_block = sb_bread(sb, cluster_list->table[cluster_index].start_bno + block_index);

                if (!bh_old_block) {
                    return -EIO;
                }

                memcpy(bh_swap_block->b_data, bh_new_block->b_data, BASICBTFS_BLOCKSIZE);
                memcpy(bh_new_block->b_data, bh_old_block->b_data, BASICBTFS_BLOCKSIZE);


                disk_block_new = (struct basicbtfs_disk_block *) bh_new->b_data;

                switch (disk_block_new->block_type_id) {
                    case BASICBTFS_BLOCKTYPE_BTREE_NODE:
                        basicbtfs_defrag_move_btree_node(sb, bh_swap_block, *offset + block_index, tmp_bno);
                        break;
                    case BASICBTFS_BLOCKTYPE_CLUSTER_TABLE:
                        basicbtfs_defrag_move_cluster_table(sb, bh_swap_block, tmp_bno);
                        break;
                    case BASICBTFS_BLOCKTYPE_NAMETREE:
                        basicbtfs_defrag_move_namelist(sb, bh_swap_block, tmp_bno);
                        break;
                    default:
                        basicbtfs_defrag_move_file_block(sb, bh_swap_block, tmp_bno);
                        break;
                }

                put_blocks(sbi, cluster_list->table[cluster_index].start_bno + block_index, 1);
                mark_buffer_dirty(bh_swap_block);
                mark_buffer_dirty(bh_old_block);
                mark_buffer_dirty(bh_new_block);
                brelse(bh_swap_block);
                brelse(bh_old_block);
                brelse(bh_new_block);
            }
        }
        cluster_list->table[cluster_index].start_bno = disk_block_offset;
        disk_block_offset += cluster_list->table[cluster_index].cluster_length;
        *offset += cluster_list->table[cluster_index].cluster_length;

    }
    return 0;
}

static inline void basicbtfs_defrag_file_cluster(void) {
    /**
     * swap with the wanted blocks. Reserve one block before the root directory to keep track of the used start of clusters.
     * In this way, we know which blocks are used as fileblock with their corresponding ino
     * 
     */


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
            basicbtfs_defrag_directory(sb, inode, offset);
        } else if (S_ISREG(inode->i_mode)) {
            basicbtfs_defrag_file_table_block(sb, inode, offset);
        }


        printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_defrag_traverse_directory(sb, node->children[index]);
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
    
    basicbtfs_defrag_btree(sb, inode, inode_info->i_bno, offset);
    basicbtfs_defrag_nametree(sb, inode, offset);

    basicbtfs_defrag_traverse_directory(sb, inode_info->i_bno, offset);

}

static inline void basicbtfs_defrag_file(void) {
    basicbtfs_defrag_file_table_block();
}

static inline void basicbtfs_defrag_disk(struct super_block *sb, struct inode *inode) {
    /**
     * 1. 
     * 3. Defrag directory
     */

    uint32_t offset = BASICBTFS_INODE(inode)->i_bno;
    basicbtfs_defrag_directory(sb, inode, &offset);

}

#endif 
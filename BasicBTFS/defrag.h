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

static inline void basicbtfs_defrag_btree_node(void) {
    /**
     * 1. If root, move btree and update inode->i_bno on disk and cache
     * 2. if non-root, move btree and update parent reference
     */
}

static inline void basicbtfs_defrag_nametree_block(struct super_block *sb, struct inode *inode, struct buffer_head *bh, uint32_t name_bno, uint32_t offset) {
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
    struct basicbtfs_disk_block *disk_block = NULL, *disk_block_next, *disk_block_prev;
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
            filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash(filename), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
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

                filename = kzalloc(sizeof(char) * cur_entry_next->name_length, GFP_KERNEL);
                strncpy(filename, block_next, cur_entry_next->name_length);
                basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash(filename), 0, name_bno, pos_next - sizeof(struct basicbtfs_name_entry));
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
    if (offset == name_bno) {
        return;
    } else if (is_bit_range_empty(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, offset, 1)) {
        uint32_t new_bno = get_offset(sbi->s_bfree_bitmap, sbi->s_bmap_blocks, offset, 1);

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

        bh_wanted = sb_bread(sb, offset);

        if (!bh_wanted) {

        }

        memcpy(bh_new->b_data, bh_wanted->b_data, BASICBTFS_BLOCKSIZE);
        memcpy(bh_wanted->b_data, bh->b_data, BASICBTFS_BLOCKSIZE);
        name_list_hdr_next->prev_block = offset;
        
        bh_prev = sb_bread(sb, name_list_hdr->prev_block);

        if (!bh_prev) {
            brelse(bh);
            brelse(bh_new);
            return -EIO;
        }

        disk_block_prev = (struct basicbtfs_disk_block *) bh_prev->b_data;
        name_list_hdr_prev = &disk_block_prev->block_type.name_list_hdr;
        
        name_list_hdr_prev->next_block = offset;
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
            filename = kzalloc(sizeof(char) * cur_entry->name_length, GFP_KERNEL);
            strncpy(filename, block, cur_entry->name_length);
            basicbtfs_btree_node_update_namelist_info(sb, BASICBTFS_INODE(inode)->i_bno, get_hash(filename), 0, name_bno, pos - sizeof(struct basicbtfs_name_entry));
            kfree(filename);
        } else {
            i--;
        }

        block += cur_entry->name_length;
        pos += cur_entry->name_length;
        cur_entry = (struct basicbtfs_name_entry *) block;
    }

    return 0;
}

static inline void basicbtfs_defrag_nametree(struct super_block *sb, inode *inode, uint32_t offset) {
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

    basicbtfs_defrag_nametree_block(sb, );

    while (name_list_hdr->next_block != 0) {
        cur_namelist_bno = name_list_hdr->next_block;
        brelse(bh);

        bh = sb_bread(sb, cur_namelist_bno);

        if (!bh) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        name_list_hdr = &disk_block->block_type.name_list_hdr;

        basicbtfs_defrag_nametree_block();
    }

    brelse(bh);

}

static inline void basicbtfs_defrag_file_table_block(void) {
    /**
     * 1. defrag filetable block and place on wanted place. If necessary, move current block based on the block filetype.
     * 2. Iterate over all file extents and swap the files if necessary.
     * end
     */
}

static inline void basicbtfs_defrag_file_cluster(void) {
    /**
     * swap with the wanted blocks. Reserve one block before the root directory to keep track of the used start of clusters.
     * In this way, we know which blocks are used as fileblock with their corresponding ino
     * 
     */
}

static inline void basicbtfs_defrag_traverse_directory(struct super_block *sb, uint32_t bno) {
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
            ret = basicbtfs_defrag_traverse_directory(sb, node->children[index]);

            if (ret != 0) {
                brelse(bh);
                return ret;
            }
        }
        
        inode = basicbtfs_iget(sb, node->entries[index].ino);
        if (S_ISDIR(inode->i_mode)) {
            basicbtfs_defrag_directory(sb, inode, );
        } else if (S_ISREG(inode->i_mode)) {
            basicbtfs_defrag_file();
        }


        printk(KERN_INFO "file: %d | ino: %d\n", node->entries[index].hash, node->entries[index].ino);
    }

    if (!node->leaf) {
        ret = basicbtfs_defrag_traverse_directory(sb, node->children[index]);
    }

    brelse(bh);
    return 0;
}

static inline void basicbtfs_defrag_directory(struct super_block *sb, struct inode *inode, uint32_t offset) {
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
    

    basicbtfs_defrag_nametree();
    basicbtfs_defrag_btree();

    basicbtfs_defrag_traverse_directory(sb, inode_info->i_bno);
    basicbtfs_defrag_traverse_directory(sb, inode_info->i_bno);

}

static inline void basicbtfs_defrag_file(void) {

}

static inline void basicbtfs_defrag_disk(void) {
    /**
     * 1. 
     * 3. Defrag directory
     */
}

#endif 
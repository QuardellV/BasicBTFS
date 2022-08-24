
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>

#include "bitmap.h"
#include "basicbtfs.h"

uint32_t basicbtfs_search_cluster(struct basicbtfs_cluster_table *cluster_table, uint32_t iblock) {
    uint32_t i = 0, len = 0, phy_block = 0, old_total_nr_of_blocks = 0, total_nr_of_blocks = 0;

    for (i = 0; i < BASICBTFS_ATABLE_MAX_CLUSTERS; i++) {
        phy_block = cluster_table->table[i].start_bno;
        len = cluster_table->table[i].cluster_length;
        total_nr_of_blocks += len;

         if (phy_block == 0 ||( iblock >= old_total_nr_of_blocks && iblock < total_nr_of_blocks)) {
            return i;
        }
    }
    
    return -1;
}

int basicbtfs_file_update_root(struct inode *inode, uint32_t bno) {
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

static int basicbtfs_file_free_blocks(struct inode *inode) {
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_inode_info *ci = BASICBTFS_INODE(inode);
    struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block;
    struct basicbtfs_block *disk_file_block;
    struct buffer_head *bh, *bh_block;
    int ret = 0, bno, i;
    uint32_t cluster_index = 0, block_index = 0, disk_block_offset;

    bh = sb_bread(sb, ci->i_bno);

    if (!bh) {
        return -EIO;
    }

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    cluster_list = &disk_block->block_type.cluster_table;

    for (cluster_index = 0; cluster_index < BASICBTFS_ATABLE_MAX_CLUSTERS; cluster_index++) {

        if (cluster_list->table[cluster_index].start_bno == 0) {
            printk("end of cluster table with index: %d\n", cluster_index);
            break;
        }
        for (block_index = 0; block_index < cluster_list->table[cluster_index].cluster_length; block_index++) {
            disk_block_offset = cluster_list->table[cluster_index].start_bno;

            bh_block = sb_bread(sb, disk_block_offset);

            if (!bh_block) return -EIO;

            disk_file_block = (struct basicbtfs_block *) bh_block->b_data;
            sbi->s_fileblock_map[disk_block_offset].cluster_index = 0;
            sbi->s_fileblock_map[disk_block_offset].ino = 0;

            memset(disk_file_block, 0, sizeof(struct basicbtfs_block));
            mark_buffer_dirty(bh_block);
            brelse(bh_block);
            put_blocks(sbi, disk_block_offset, 1);
            // if is empty take
            // else if-not empty swap
            // printk("index, bno, and length: %d | %d | %d\n", cluster_index, disk_block_offset, cluster_list->table[cluster_index].cluster_length);
          
        }
    }
    memset(disk_block, 0, sizeof(struct basicbtfs_disk_block));
    mark_buffer_dirty(bh);
    brelse(bh);
    put_blocks(sbi,bno, 1);
    return 0;
}

static int basicbtfs_file_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create) {
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_inode_info *ci = BASICBTFS_INODE(inode);
    // struct basicbtfs_cluster_table *cluster_list;
    struct basicbtfs_disk_block *disk_block;
    struct buffer_head *bh_index;
    int ret = 0, bno, i;
    uint32_t cluster_index = 0;

    if (iblock >= BASICBTFS_ATABLE_MAX_CLUSTERS) {
        return -EFBIG;
    }

    bh_index = sb_bread(sb, ci->i_bno);

    if (!bh_index) {
        brelse(bh_index);
        return -EIO;
    }

    disk_block = (struct basicbtfs_disk_block *) bh_index->b_data;

    // cluster_list = (struct basicbtfs_cluster_table *) bh_index->b_data;
    cluster_index = basicbtfs_search_cluster(&disk_block->block_type.cluster_table, iblock);

    if (disk_block->block_type.cluster_table.table[cluster_index].start_bno == 0) {
        if (!create) {
            return ret;
        }
        
        bno = get_free_blocks(sbi, BASICBTFS_MAX_BLOCKS_PER_CLUSTER);
        if (!bno) {
            brelse(bh_index);
            return -ENOSPC;
        }
        printk("new block: %d till %d\n", bno, bno + 3);
        disk_block->block_type.cluster_table.table[cluster_index].start_bno = bno;
        disk_block->block_type.cluster_table.table[cluster_index].cluster_length = BASICBTFS_MAX_BLOCKS_PER_CLUSTER;

        for (i = 0; i < BASICBTFS_MAX_BLOCKS_PER_CLUSTER; i++) {
            sbi->s_fileblock_map[bno + i].ino = inode->i_ino;
            sbi->s_fileblock_map[bno + i].cluster_index = cluster_index;
        }
        // inode->i_blocks += 1;
    } else {
        bno = disk_block->block_type.cluster_table.table[cluster_index].start_bno + (iblock % disk_block->block_type.cluster_table.table[cluster_index].cluster_length);
    }
    // printk("basicbtfs_file_get_block() sb_bread bno: %d | %lld \n", bno, iblock);
    map_bh(bh_result, sb, bno);
    return ret;
}

static int basicbtfs_readpage(struct file *file, struct page *page) {
    // printk(KERN_INFO "basicftfs_readpage()");
    return mpage_readpage(page, basicbtfs_file_get_block);
}

static int basicbtfs_writepage(struct page *page, struct writeback_control *wbc) {
    // printk(KERN_INFO "basicftfs_write_page()");
    return block_write_full_page(page, basicbtfs_file_get_block, wbc);
}

static int basicbtfs_write_begin(struct file *file,
                                struct address_space *mapping,
                                loff_t pos,
                                unsigned int len,
                                unsigned int flags,
                                struct page **pagep,
                                void **fsdata) {
    int err;
    printk(KERN_INFO "basicftfs_write_begin()");

    /* prepare the write */
    err = block_write_begin(mapping, pos, len, flags, pagep, basicbtfs_file_get_block);
    /* if this failed, reclaim newly allocated blocks */
    if (err < 0) {
        printk(KERN_ERR "TODO");
    }
    return err;
}

static int basicbtfs_write_end(struct file *file,
                              struct address_space *mapping,
                              loff_t pos,
                              unsigned int len,
                              unsigned int copied,
                              struct page *page,
                              void *fsdata) {
    struct inode *inode = file->f_inode;

    int ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
    printk(KERN_INFO "basicftfs_write_end()");
    if (ret < len) {
        pr_err("wrote less than requested.");
        return ret;
    }

    inode->i_mtime = inode->i_ctime = current_time(inode);
    mark_inode_dirty(inode);

    return ret;
}

const struct address_space_operations basicbtfs_aops = {
    .readpage = basicbtfs_readpage,
    .writepage = basicbtfs_writepage,
    .write_begin = basicbtfs_write_begin,
    .write_end = basicbtfs_write_end,
};

const struct file_operations basicbtfs_file_ops = {
    .llseek = generic_file_llseek,
    .owner = THIS_MODULE,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .fsync = generic_file_fsync,
    .unlocked_ioctl = basicbtfs_ioctl,
};
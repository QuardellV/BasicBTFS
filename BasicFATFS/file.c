
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>

#include "bitmap.h"
#include "basicftfs.h"

static int basicftfs_file_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create) {
    struct super_block *sb = inode->i_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct basicftfs_inode_info *ci = BASICFTFS_INODE(inode);
    struct basicftfs_alloc_table *block_list;
    struct buffer_head *bh_index;
    int ret = 0, bno;

    if (iblock >= BASICFTFS_ATABLE_MAX_BLOCKS) {
        return -EFBIG;
    }

    bh_index = sb_bread(sb, ci->i_bno);

    if (!bh_index) {
        brelse(bh_index);
        return -EIO;
    }
    
    block_list = (struct basicftfs_alloc_table *) bh_index->b_data;

    if (block_list->table[iblock] == 0) {
        if (!create) {
            return ret;
        }

        bno = get_free_blocks(sbi, 1);
        if (!bno) {
            brelse(bh_index);
            return -ENOSPC;
        }
        block_list->table[iblock] = bno;
    } else {
        bno = block_list->table[iblock];
    }
    
    map_bh(bh_result, sb, bno);
    return ret;
}

static int basicftfs_readpage(struct file *file, struct page *page) {
    return mpage_readpage(page, basicftfs_file_get_block);
}

static int basicftfs_writepage(struct page *page, struct writeback_control *wbc) {
    return block_write_full_page(page, basicftfs_file_get_block, wbc);
}

static int basicftfs_write_begin(struct file *file,
                                struct address_space *mapping,
                                loff_t pos,
                                unsigned int len,
                                unsigned int flags,
                                struct page **pagep,
                                void **fsdata) {
    int err;

    /* prepare the write */
    err = block_write_begin(mapping, pos, len, flags, pagep, basicftfs_file_get_block);
    /* if this failed, reclaim newly allocated blocks */
    if (err < 0) {
        printk(KERN_ERR "TODO");
    }
    return err;
}

static int basicftfs_write_end(struct file *file,
                              struct address_space *mapping,
                              loff_t pos,
                              unsigned int len,
                              unsigned int copied,
                              struct page *page,
                              void *fsdata) {
    struct inode *inode = file->f_inode;

    int ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
    if (ret < len) {
        pr_err("wrote less than requested.");
        return ret;
    }

    inode->i_mtime = inode->i_ctime = current_time(inode);
    mark_inode_dirty(inode);

    return ret;
}

const struct address_space_operations basicftfs_aops = {
    .readpage = basicftfs_readpage,
    .writepage = basicftfs_writepage,
    .write_begin = basicftfs_write_begin,
    .write_end = basicftfs_write_end,
};

const struct file_operations basicftfs_file_ops = {
    .llseek = generic_file_llseek,
    .owner = THIS_MODULE,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .fsync = generic_file_fsync,
};
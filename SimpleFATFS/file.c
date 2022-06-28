#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mpage.h>

#include "basicftfs.h"

const struct address_space_operations basicftfs_aops = {
    // .readpage = basicfs_readpage,
    // .writepage = basicfs_writepage,
    // .write_begin = basicfs_write_begin,
    // .write_end = basicfs_write_end,
};

const struct file_operations basicftfs_file_ops = {
    .llseek = generic_file_llseek,
    .owner = THIS_MODULE,
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .fsync = generic_file_fsync,
};

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"

const struct file_operations basicftfs_dir_ops = {
    // .owner = THIS_MODULE,
    // .iterate_shared = basicfs_iterate,
};
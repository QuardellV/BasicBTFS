#ifndef BASICBTFS_INIT_H
#define BASICBTFS_INIT_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "basicbtfs.h"
#include "io.h"

static inline void init_mode_attributes(mode_t mode, struct inode *vfs_inode, struct basicbtfs_inode_info *inode_info, uint32_t bno) {
    vfs_inode->i_blocks = 1;
    if (S_ISDIR(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = BASICBTFS_BLOCKSIZE;
        vfs_inode->i_fop = &basicbtfs_dir_ops;
        set_nlink(vfs_inode, 2); /* . and .. */
    } else if (S_ISREG(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = 0;
        vfs_inode->i_fop = &basicbtfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicbtfs_aops;
        set_nlink(vfs_inode, 1);
    }
}

static inline void init_inode_mode(struct inode *vfs_inode, struct basicbtfs_inode *disk_inode, struct basicbtfs_inode_info *inode_info) {
    if (S_ISDIR(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicbtfs_dir_ops;
    } else if (S_ISREG(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicbtfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicbtfs_aops;
    }
}

static inline void my_get_rand_bytes(char *buffer, int num) {
    get_random_bytes(buffer, num);

    printk(KERN_INFO "random bytes: %s\n", buffer);
}

static inline unsigned long get_hash(struct dentry *dentry, const char *salt) {
    const int length = BASICBTFS_NAME_LENGTH + strlen(salt);
    char *tmp_buffer = kzalloc(sizeof(char) * length, GFP_KERNEL);
    u32 crc = 0;

    memcpy(tmp_buffer, dentry->d_name.name, BASICBTFS_NAME_LENGTH);
    memcpy(tmp_buffer + BASICBTFS_NAME_LENGTH, salt, strlen(salt));

    crc = crc32(crc, tmp_buffer, length);

    printk(KERN_INFO "Current unsigned long: %hhx\n", crc);

    kfree(tmp_buffer);

    return crc;
}

#endif

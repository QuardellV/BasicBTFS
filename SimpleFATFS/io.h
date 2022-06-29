#ifndef BASICFTFS_IO_H
#define BASICFTFS_IO_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"

static void write_from_disk_to_vfs_inode(struct super_block *sb, struct inode *vfs_inode, struct basicftfs_inode *disk_inode, unsigned long ino) {
    vfs_inode->i_ino = ino;
    vfs_inode->i_sb = sb;
    vfs_inode->i_op = &basicftfs_inode_ops;

    vfs_inode->i_mode = le32_to_cpu(disk_inode->i_mode);
    i_uid_write(vfs_inode, le32_to_cpu(disk_inode->i_uid));
    i_gid_write(vfs_inode, le32_to_cpu(disk_inode->i_gid));
    vfs_inode->i_size = le32_to_cpu(disk_inode->i_size);
    vfs_inode->i_ctime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_ctime);
    vfs_inode->i_ctime.tv_nsec = vfs_inode->i_atime.tv_nsec = vfs_inode->i_mtime.tv_nsec = 0;
    vfs_inode->i_atime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_atime);
    vfs_inode->i_mtime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_mtime);
    vfs_inode->i_blocks = le32_to_cpu(disk_inode->i_blocks);
    set_nlink(vfs_inode, le32_to_cpu(disk_inode->i_nlink));
}

#endif
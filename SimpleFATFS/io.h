#ifndef BASICFTFS_IO_H
#define BASICFTFS_IO_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"

static inline void write_from_disk_to_vfs_inode(struct super_block *sb, struct inode *vfs_inode, struct basicftfs_inode *disk_inode, unsigned long ino) {
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

static inline void write_from_vfs_inode_to_disk(struct basicftfs_inode *disk_inode, struct inode *vfs_inode) {
    struct basicftfs_inode_info *inode_info = BASICFTFS_INODE(vfs_inode);

    disk_inode->i_mode = vfs_inode->i_mode;
    disk_inode->i_uid = i_uid_read(vfs_inode);
    disk_inode->i_gid = i_gid_read(vfs_inode);
    disk_inode->i_size = vfs_inode->i_size;
    disk_inode->i_ctime = vfs_inode->i_ctime.tv_sec;
    disk_inode->i_atime = vfs_inode->i_atime.tv_sec;
    disk_inode->i_mtime = vfs_inode->i_mtime.tv_sec;
    disk_inode->i_blocks = vfs_inode->i_blocks;
    disk_inode->i_nlink = vfs_inode->i_nlink;
    disk_inode->i_bno = inode_info->i_bno;
    strncpy(disk_inode->i_data, inode_info->i_data, sizeof(inode_info->i_data));
}

static inline int flush_superblock(struct super_block *sb, int wait) {
    struct buffer_head *bh = NULL;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct basicftfs_sb_info *disk_sbi = NULL;

    bh = sb_bread(sb, BASICFTFS_SB_BNO);

    if (!bh) return -EIO;

    disk_sbi = (struct basicftfs_sb_info *) bh->b_data;

    disk_sbi->s_nblocks = sbi->s_nblocks;
    disk_sbi->s_ninodes = sbi->s_ninodes;
    disk_sbi->s_inode_blocks = sbi->s_inode_blocks;
    disk_sbi->s_imap_blocks = sbi->s_imap_blocks;
    disk_sbi->s_bmap_blocks = sbi->s_bmap_blocks;
    disk_sbi->s_nfree_inodes = sbi->s_nfree_inodes;
    disk_sbi->s_nfree_blocks = sbi->s_nfree_blocks;

    mark_buffer_dirty(bh);
    if (wait) sync_dirty_buffer(bh);
    brelse(bh);

    return 0;
}

static inline int flush_bitmap(struct super_block *sb, unsigned long *bitmap, uint32_t map_nr_blocks, uint32_t block_offset, int wait) {
    struct buffer_head *bh = NULL;
    int i = 0;

    for (i = 0; i < map_nr_blocks; i++) {
        int idx = block_offset + i;
        bh = sb_bread(sb, idx);

        if (!bh) return -EIO;

        memcpy(bh->b_data, (void *) bitmap + i * BASICFTFS_BLOCKSIZE, BASICFTFS_BLOCKSIZE);

        mark_buffer_dirty(bh);
        if (wait) sync_dirty_buffer(bh);
        brelse(bh);
    }
    return 0;
}

static inline void move_files(struct basicftfs_entry *fblock_prev, struct basicftfs_entry *fblock_cur) {
    memmove(fblock_prev + BASICFTFS_ENTRIES_PER_BLOCK - 1, fblock_cur, sizeof(struct basicftfs_entry));
    memmove(fblock_cur, fblock_cur + 1, (BASICFTFS_ENTRIES_PER_BLOCK - 1) * sizeof(struct basicftfs_entry));
    memset(fblock_cur + BASICFTFS_ENTRIES_PER_BLOCK - 1, 0, sizeof(struct basicftfs_entry));
}

static inline void remove_file_from_dir(int fi, struct basicftfs_entry *fblock) {
    if (fi != BASICFTFS_ENTRIES_PER_BLOCK - 1) {
        memmove(fblock + fi, fblock + fi + 1, (BASICFTFS_ENTRIES_PER_BLOCK - fi - 1) * sizeof(struct basicftfs_entry));
    }
    memset(fblock + BASICFTFS_ENTRIES_PER_BLOCK - 1, 0, sizeof(struct basicftfs_entry));
}

#endif
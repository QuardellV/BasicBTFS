#ifndef BASICBTFS_BITMAP_H
#define BASICBTFS_BITMAP_H

#include <linux/bitmap.h>
#include "basicbtfs.h"

static inline uint32_t get_first_free_bits(unsigned long *freemap, unsigned long size, uint32_t len) {
    unsigned long start_no = bitmap_find_next_zero_area(freemap, size, 1, len, 0);

    if (start_no >= size) {
        printk(KERN_ERR "no free area has been found\n");
        return -1;
    }
    bitmap_set(freemap, start_no, len);
    return start_no;
}

static inline uint32_t get_free_inode(struct basicbtfs_sb_info *sbi) {
    uint32_t start_ino = get_first_free_bits(sbi->s_ifree_bitmap, sbi->s_ninodes, 1);

    if (start_ino > 0) {
        sbi->s_nfree_inodes--;
    }

    return start_ino;
}

static inline uint32_t get_free_blocks(struct basicbtfs_sb_info *sbi, uint32_t len) {
    uint32_t start_bno = get_first_free_bits(sbi->s_bfree_bitmap, sbi->s_nblocks, len);
    if (start_bno > 0) {
        sbi->s_nfree_blocks -= len;
    }

    return start_bno;
}

static inline int put_free_bits(unsigned long *freemap, unsigned long size, uint32_t start_no, uint32_t len) {
    if (start_no + len - 1 > size) return -1;

    bitmap_clear(freemap, start_no, len);
    return 0;
}

static inline void put_inode(struct basicbtfs_sb_info *sbi, uint32_t ino) {
    int ret = put_free_bits(sbi->s_ifree_bitmap, sbi->s_ninodes, ino, 1);
    if (ret != 0) return;

    sbi->s_nfree_inodes++;
}

static inline void put_blocks(struct basicbtfs_sb_info *sbi,uint32_t bno, uint32_t len) {
    int ret = put_free_bits(sbi->s_bfree_bitmap, sbi->s_nblocks, bno, len);

    if (ret != 0) return;

    sbi->s_nfree_blocks += len;
}

#endif /* BASICBTFS_BITMAP_H */
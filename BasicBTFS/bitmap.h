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


static inline bool is_bit_range_empty(unsigned long *freemap, unsigned long size, unsigned long start, uint32_t len) {
    unsigned long start_no = bitmap_find_next_zero_area(freemap, size, start, len, 0);

    return start_no == start;
}

static inline bool is_bit_empty(unsigned long *freemap, unsigned long size, unsigned long start, uint32_t block_index) {
    unsigned char *cur_map = (unsigned char *)freemap;
    uint32_t index = start / (sizeof(unsigned char) * 8);
    uint32_t offset = start % (sizeof(unsigned char) * 8);

    // if (block_index > 2) return false;

    if (!(cur_map[index] & (1 << offset))) {
        printk("yay 0\n");
    } else {
        printk("no 1");
    }
    return !(cur_map[index] & (1 << offset));
}

static inline bool is_bit_range_empty_test(unsigned long *freemap, unsigned long size, unsigned long start, uint32_t len, uint32_t index) {
    unsigned long start_no = bitmap_find_next_zero_area(freemap, size, start, len, 0);

    return start_no == start;
}

static inline uint32_t get_offset(struct basicbtfs_sb_info *sbi, unsigned long *freemap, unsigned long size, unsigned long start, uint32_t len) {
    unsigned long start_no = bitmap_find_next_zero_area(freemap, size, start, len, 0);

    if (start_no >= size) {
        printk(KERN_ERR "no free area has been found: %ld\n", start_no);
        return -1;
    }
    
    bitmap_set(freemap, start_no, len);

    if (start_no >= sbi->s_unused_area) {
        sbi->s_unused_area = start_no + len;
    }
    return start_no;
}


static inline uint32_t get_first_free_bits_from_start(unsigned long *freemap, unsigned long size, unsigned long start, uint32_t len) {
    unsigned long start_no = bitmap_find_next_zero_area(freemap, size, start, len, 0);

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
    printk("start bno: %d | %d\n", start_bno, len);
    if (start_bno > 0) {
        sbi->s_nfree_blocks -= len;
    }

    if (start_bno >= sbi->s_unused_area) {
        sbi->s_unused_area = start_bno + len;
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
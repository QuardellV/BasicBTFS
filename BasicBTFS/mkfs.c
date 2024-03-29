#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>

#include "basicbtfs.h"

struct superblock {
    struct basicbtfs_sb_info info;
    char padding[BASICBTFS_BLOCKSIZE - sizeof(struct basicbtfs_sb_info)]; /* Padding to match block size */
};

/* Returns ceil(a/b) */
static inline uint32_t div_ceil(uint32_t a, uint32_t b) {
    uint32_t ret = a / b;
    if (a % b) {
        return ret + 1;
    } else {
        return ret;
    }
}

static void my_set_bit(uint64_t *val, int bit, int value) {
    if(value) {
        *val |= ((uint64_t)1 << bit);
    } else {
        *val &= ~((uint64_t)1 << bit);
    }
}

static struct superblock *write_superblock(int fd, struct stat *fstats) {
    struct superblock *sb = calloc(1, sizeof(struct superblock));
    if (!sb) {
        return NULL;
    }

    uint32_t nr_blocks = fstats->st_size / BASICBTFS_BLOCKSIZE;
    uint32_t nr_inodes = nr_blocks;
    uint32_t nr_ifree_bmap_blocks = div_ceil(nr_inodes, BASICBTFS_BLOCKSIZE * 8);
    uint32_t nr_bfree_bmap_blocks = div_ceil(nr_blocks, BASICBTFS_BLOCKSIZE * 8);
    uint32_t nr_file_map_blocks  = div_ceil(nr_blocks, BASICBTFS_WORDS_PER_BLOCK);
    uint32_t nr_inode_blocks = div_ceil(nr_inodes, BASICBTFS_INODES_PER_BLOCK);
    uint32_t nr_data_blocks = nr_blocks - nr_ifree_bmap_blocks - nr_bfree_bmap_blocks - nr_inode_blocks - nr_file_map_blocks;

    sb->info.s_magic = htole32(BASICBTFS_MAGIC_NUMBER);
    sb->info.s_nblocks = htole32(nr_blocks);
    sb->info.s_ninodes = htole32(nr_inodes);
    sb->info.s_inode_blocks = htole32(nr_inode_blocks);
    sb->info.s_imap_blocks = htole32(nr_ifree_bmap_blocks);
    sb->info.s_bmap_blocks = htole32(nr_bfree_bmap_blocks);
    sb->info.s_filemap_blocks = htole32(nr_file_map_blocks);
    sb->info.s_nfree_inodes = htole32(nr_inodes - 1);
    sb->info.s_nfree_blocks = htole32(nr_data_blocks - 1);

    int ret = write(fd, sb, sizeof(struct superblock));
    if (ret != sizeof(struct superblock)) {
        free(sb);
        return NULL;
    }

    return sb;
}

static int write_ifree_blocks(int fd, struct superblock *sb) {
    char block[BASICBTFS_BLOCKSIZE];
    memset(block, 0, BASICBTFS_BLOCKSIZE);

    uint64_t *ifree = (uint64_t *) block;
    ifree[0] = htole64(0x1);
    int ret = write(fd, ifree, BASICBTFS_BLOCKSIZE);
    if (ret != BASICBTFS_BLOCKSIZE) {
        return -1;
    }

    memset(ifree, 0, BASICBTFS_BLOCKSIZE);
    uint32_t i;
    for (i = 1; i < le32toh(sb->info.s_imap_blocks); i++) {
        ret = write(fd, ifree, BASICBTFS_BLOCKSIZE);
        if (ret != BASICBTFS_BLOCKSIZE) {
            return -1;
        }
    }

    return 0;
}


static int write_bfree_blocks(int fd, struct superblock *sb)
{
    uint32_t bits_used = le32toh(sb->info.s_imap_blocks) + le32toh(sb->info.s_bmap_blocks) + le32toh(sb->info.s_inode_blocks) +  le32toh(sb->info.s_filemap_blocks) + 2;
    uint32_t blocks_used = bits_used / BASICBTFS_BLOCKSIZE;
    uint32_t used_lines_last = (bits_used % BASICBTFS_BLOCKSIZE) / 64;
    uint32_t used_lines_rem_last = (bits_used % BASICBTFS_BLOCKSIZE) % 64; // if not all 64 bits all free, this is are the first n not free bits.
    int ret = 0;

    char block[BASICBTFS_BLOCKSIZE];
    memset(block, 0, BASICBTFS_BLOCKSIZE);

    uint64_t *bfree = (uint64_t *) block;

    uint32_t i = 0;

    for (i = 0; i < blocks_used; i++) {
        if (blocks_used - 1 == i) {
            memset(bfree, 0, BASICBTFS_BLOCKSIZE);
            uint32_t i = 0;
            uint64_t line = 0xffffffffffffffff;

            for (i = 0; i < used_lines_last; i++) {
                bfree[i] = htole64(line);
                bits_used -= 64;
            }

            uint64_t test = 0;

            for (i = 0; i < used_lines_rem_last; i++) {
                my_set_bit(&test, i, 1);
            }

            bfree[used_lines_last] = htole64(test);

            ret = write(fd, bfree, BASICBTFS_BLOCKSIZE);
            if (ret != BASICBTFS_BLOCKSIZE) {
                return -1;
            }

        } else {
            memset(block, 0xFF, BASICBTFS_BLOCKSIZE);
            ret = write(fd, block, BASICBTFS_BLOCKSIZE);

            if (ret != BASICBTFS_BLOCKSIZE) {
                return -1;
            }
        }
    }

    memset(bfree, 0, BASICBTFS_BLOCKSIZE);
    for (i = blocks_used; i < le32toh(sb->info.s_bmap_blocks); i++) {
        ret = write(fd, bfree, BASICBTFS_BLOCKSIZE);
        if (ret != BASICBTFS_BLOCKSIZE) {
            return -1;
        }
        printf("succesfully written block: %d\n", i);
    }

    printf("Block bitmap has %d blocks\n", i);
    return 0;
}

static int write_filemap_blocks(int fd, struct superblock *sb) {
    int i = 0, ret = 0;
    char block[BASICBTFS_BLOCKSIZE];
    memset(block, 0, BASICBTFS_BLOCKSIZE);

    for (i = 0; i < le32toh(sb->info.s_filemap_blocks); i++) {
        ret = write(fd, block, BASICBTFS_BLOCKSIZE);
        if (ret != BASICBTFS_BLOCKSIZE) {
            return -1;
        }
    }

    return 0;
}

static int write_inode_blocks(int fd, struct superblock *sb) {
    char block[BASICBTFS_BLOCKSIZE];
    memset(block, 0, BASICBTFS_BLOCKSIZE);

    struct basicbtfs_inode *inode = (struct basicbtfs_inode *) block;
    uint32_t first_data_block = 1 + le32toh(sb->info.s_imap_blocks) + le32toh(sb->info.s_bmap_blocks) + le32toh(sb->info.s_inode_blocks) + le32toh(sb->info.s_filemap_blocks);
    inode->i_mode = htole32(S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH); /* All chmod permissions are allowed */
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->i_size = htole32(BASICBTFS_BLOCKSIZE);
    inode->i_ctime = inode->i_atime = inode->i_mtime = htole32(0);
    inode->i_blocks = htole32(1);
    inode->i_nlink = htole32(2);
    inode->i_bno = htole32(first_data_block);

    int ret = write(fd, block, BASICBTFS_BLOCKSIZE);
    if (ret != BASICBTFS_BLOCKSIZE) {
        return -1;
    }

    memset(block, 0, BASICBTFS_BLOCKSIZE);

    uint32_t i;
    for (i = 1; i < sb->info.s_inode_blocks; i++) {
        ret = write(fd, block, BASICBTFS_BLOCKSIZE);
        if (ret != BASICBTFS_BLOCKSIZE) {
            return -1;
        }
    }

    return 0;
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        perror("Not correct amount of arguments\n");
        return EXIT_FAILURE;
    }

    /* Open disk image */
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("could not open disk\n");
        return EXIT_FAILURE;
    }

    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);
    if (ret) {
        perror("fstat():");
        close(fd);
        return EXIT_FAILURE;
    }

    if ((stat_buf.st_mode & S_IFMT) == S_IFBLK) {
        long int blk_size = 0;
        ret = ioctl(fd, BLKGETSIZE64, &blk_size);
        if (ret != 0) {
            perror("get block size faied:");
            close(fd);
            return EXIT_FAILURE;
        }
        stat_buf.st_size = blk_size;
    }

    struct superblock *sb = write_superblock(fd, &stat_buf);
    if (!sb) {
        perror("write_superblock() failed:");
        close(fd);
        return EXIT_FAILURE;
    }

    ret = write_ifree_blocks(fd, sb);
    if (ret) {
        perror("write_ifree_bmap_block() failed");
        free(sb);
        close(fd);
        return EXIT_FAILURE;
    }

    ret = write_bfree_blocks(fd, sb);
    if (ret) {
        perror("write_bfree_bmap_block() failed");
        free(sb);
        close(fd);
        return EXIT_FAILURE;
    }

    ret = write_inode_blocks(fd, sb);
    if (ret) {
        perror("write_inode_blocks() failed:");
        free(sb);
        close(fd);
        return EXIT_FAILURE;
    }

    ret = write_filemap_blocks(fd, sb);
    if (ret) {
        perror("write_filemap_block() failed");
        free(sb);
        close(fd);
        return EXIT_FAILURE;
    }

    free(sb);
    close(fd);
    return ret;
}

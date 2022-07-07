#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>

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

    // struct stat stat_buf;
    // int ret = fstat(fd, &stat_buf);
    // if (ret) {
    //     perror("fstat():");
    //     close(fd);
    //     return EXIT_FAILURE;
    // }

    // if ((stat_buf.st_mode & S_IFMT) == S_IFBLK) {
    //     long int blk_size = 0;
    //     ret = ioctl(fd, BLKGETSIZE64, &blk_size);
    //     if (ret != 0) {
    //         perror("get block size faied:");
    //         close(fd);
    //         return EXIT_FAILURE;
    //     }
    //     stat_buf.st_size = blk_size;
    // }

    // struct superblock *sb = write_superblock(fd, &stat_buf);
    // if (!sb) {
    //     perror("write_superblock() failed:");
    //     close(fd);
    //     return EXIT_FAILURE;
    // }

    // ret = write_ifree_blocks(fd, sb);
    // if (ret) {
    //     perror("write_ifree_bmap_block() failed");
    //     free(sb);
    //     close(fd);
    //     return EXIT_FAILURE;
    // }

    // ret = write_bfree_blocks(fd, sb);
    // if (ret) {
    //     perror("write_bfree_bmap_block() failed");
    //     free(sb);
    //     close(fd);
    //     return EXIT_FAILURE;
    // }

    // ret = write_inode_blocks(fd, sb);
    // if (ret) {
    //     perror("write_inode_blocks() failed:");
    //     free(sb);
    //     close(fd);
    //     return EXIT_FAILURE;
    // }

    // free(sb);
    close(fd);
    return 0;
}

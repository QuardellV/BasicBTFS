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

    close(fd);
    return 0;
}

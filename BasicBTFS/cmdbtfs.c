#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stdint.h>


#include "basicbtfs.h"

#define BASICBTFS_LOOP_DEVICE "/BasicBTFS/"

struct command_t {
    char *name;
    unsigned long code;
};

struct command_list_t {
    struct command_t **data;
    int pointer;
    int size;
};

struct command_list_t *cmd_list;

void init_command(char *name, unsigned long code) {
    cmd_list->pointer++;

    struct command_t *new_command = malloc(sizeof(struct command_t));
    new_command->name = name;
    new_command->code = code;

    if (cmd_list->pointer >= cmd_list->size) {
        cmd_list->size = 2 * cmd_list->pointer;
        cmd_list->data = realloc(cmd_list->data, (size_t)(cmd_list->size) * sizeof(struct command_t)); // +1 prevents address leak! https://stackoverflow.com/questions/51579267/addresssanitizer-heap-buffer-overflow-on-address
    }

    cmd_list->data[cmd_list->pointer] = new_command;
}

void init_default_commands() {

    cmd_list = malloc(sizeof(struct command_list_t));
    cmd_list->size = 128;
    cmd_list->pointer = -1;
    cmd_list->data = calloc((size_t) cmd_list->size, sizeof(struct command_t));

    init_command("defragment", BASICBTFS_IOC_DEFRAG);
    init_command("defrag", BASICBTFS_IOC_DEFRAG);
    // init_command("quit"     , CMD_QUIT);
    // init_command("q"        , CMD_QUIT);
    // init_command("help"     , CMD_HELP);
    // init_command("h"        , CMD_HELP);
    // init_command("file"     , CMD_FILE);
    // init_command("run"      , CMD_RUN);
    // init_command("r"        , CMD_RUN);
    // init_command("input"    , CMD_INPUT);
    // init_command("break"    , CMD_BREAK);
    // init_command("b"        , CMD_BREAK);
    // init_command("step"     , CMD_STEP);
    // init_command("s"        , CMD_STEP);
    // init_command("continue" , CMD_CONTINUE);
    // init_command("c"        , CMD_CONTINUE);
    // init_command("info"     , CMD_INFO);
    // init_command("backtrace", CMD_BACKTRACE);
    // init_command("bt"       , CMD_BACKTRACE);

}

unsigned long search_command(char *command) {
    for (int index = 0; index <= cmd_list->pointer; index++) {
        struct command_t *cur_command = cmd_list->data[index];
        printf("cur: %s, %s\n", cur_command->name, command);

        if (strcmp(command, cur_command->name) == 0) {
            return cur_command->code;
        }
    }
    return 0x00;
}

char *to_lowercase(char *command) {
    for (int index = 0; index < strlen(command); index++) {
        command[index] = tolower(command[index]);
    }
    return command;
}

int main(int argc, char **argv) {
    unsigned long cur_cmd_code = 0;
    int fd, ret;
    struct basicbtfs_ioctl_vol_args args;
    init_default_commands();

    if (argv[1] != NULL) {
        cur_cmd_code = search_command(to_lowercase(argv[1]));
    } else {
        printf("No argument given\n");
        return 0;
    }

    switch (cur_cmd_code) {
        case BASICBTFS_IOC_DEFRAG:
            if (argv[2] != NULL) {
                printf("%s will be defragmented\n", argv[2]);
                memcpy(args.name, argv[2], strlen(argv[2]) + 1);
                fd = open(args.name, O_RDONLY);

                // if (fd == -1) {
                //     perror("could not open disk\n");
                //     return EXIT_FAILURE;
                // } else {
                //     printf("yes boys\n");
                // }
                ret = ioctl(fd, cur_cmd_code, (int32_t *) &args);

                if (ret != -1) {
                    printf("oh no, something went wrong\n");
                } else {
                    printf("sent\n");
                }
            } else {
                printf("no valid entry\n");
            }
            break;
        default:
            printf("invalid command, try again\n");
    }

    if (argv[2] != NULL && argv[2][0] == '/') {
        memcpy(args.name, argv[2], strlen(argv[2]) + 1);
        ioctl(fd, cur_cmd_code, (int32_t *) &args);
    }

    close(fd);

}
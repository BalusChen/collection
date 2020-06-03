
/*
 * Copyright (C) Jianyong Chen
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>


int
main(int argc, char **argv)
{
    int  notify_fd;

    notify_fd = eventfd(0, 0);

    if (notify_fd == -1) {
        perror("eventfd() failed");
        exit(EXIT_FAILURE);
    }

    printf("notify_fd: %d\n", notify_fd);

    exit(EXIT_SUCCESS);
}

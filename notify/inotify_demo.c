
/*
 * Copyright (C) Jianyong Chen
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/epoll.h>

#define MAX_LINE                    1024
#define MAX_EPOLL_EVENTS            10
#define INOTIFY_EVENTS_BUF_SIZE     (sizeof(struct inotify_event) * 10)


static int handle_stdin(int fd);
static int handle_inotify(int fd);
static void dump_inotify_event(struct inotify_event *ie);


int
main(int argc, char **argv)
{
    int                 wd, i, nfds, ret, epfd, infd, flags;
    struct epoll_event  ee, events[MAX_EPOLL_EVENTS];

    epfd = epoll_create(2);
    if (epfd == -1) {
        perror("[inotify] epoll_create() failed\n");
        exit(EXIT_FAILURE);
    }

    infd = inotify_init1(0);
    if (infd == -1) {
        perror("[inotify] inotify_init1(0) failed\n");
        exit(EXIT_FAILURE);
    }

    ee.events = EPOLLIN;
    ee.data.fd = STDIN_FILENO;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ee) < 0) {
        perror("[inotify] epoll_ctl(ADD, stdin) failed\n");
        exit(EXIT_FAILURE);
    }

    ee.events = EPOLLIN;
    ee.data.fd = infd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, infd, &ee) < 0) {
        perror("[inotify] epoll_ctl(ADD, inotify_fd) failed\n");
        exit(EXIT_FAILURE);
    }

    // flags = IN_CREATE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_DELETE_SELF;
    flags = IN_ALL_EVENTS;

    for (i = 1; i < argc; i++) {
        wd = inotify_add_watch(infd, argv[i], flags);
        if (wd < 0) {
            perror("[inotify] inotify_add_watch() failed\n");
            exit(EXIT_FAILURE);
        }

        printf("[inotify] watch: %s\n", argv[i]);
    }

    printf("[inotify] start to listen, enter \"stop\"<enter> to leave...\n");

    for ( ;; ) {
        nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);
        if (nfds == -1) {
            perror("[inotify] epoll_wait() failed\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < nfds; i++) {

            if (events[i].data.fd == STDIN_FILENO) {
                handle_stdin(STDIN_FILENO);

            } else if (events[i].data.fd == infd) {
                handle_inotify(infd);

            } else {
                fprintf(stderr, "[inotify] unknown file descriptor: %d\n",
                        events[i].data.fd);

                exit(EXIT_FAILURE);
            }
        }
    }

    exit(EXIT_SUCCESS);

    return 0;
}


static int
handle_stdin(int fd)
{
    int   n, size;
    char  *p, buf[MAX_LINE];

    for (p = buf, size = sizeof(buf); size != 0; /* void */ ) {
        n = read(fd, buf, size);
        if (n < 0) {
            perror("[inotify] read from stdin failed");
            exit(EXIT_FAILURE);

        } else if (n == 0) {

            if ((p - buf) != sizeof("stop")
                || strncmp(buf, "stop", sizeof("stop")) == 0)
            {
                printf("[inotify] read from stdin: \"%s\", unexpected message\n", buf);
                break;
            }

            printf("[inotify] bye bye...\n");
            exit(EXIT_SUCCESS);

        } else {
            p += n;
            size -= n;
            if (size == 0) {
                fprintf(stderr, "[inotify] input too long\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;
}


static int
handle_inotify(int fd)
{
    int                    i, n;
    char                  *p;
    struct inotify_event  *ie;

    char buf[INOTIFY_EVENTS_BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    n = read(fd, buf, sizeof(buf));
    if (n == -1) {
        perror("[inotify] read from inotify fd failed\n");
        exit(EXIT_FAILURE);
    }

    for (p = buf; p + sizeof(struct inotify_event) <= buf + n; /* void */) {
        ie = (struct inotify_event *) p;

        dump_inotify_event(ie);

        p += (sizeof(struct inotify_event) + ie->len);
    }

    return 0;
}

static void
dump_inotify_event(struct inotify_event *ie)
{
    /* TODO: implement */

    printf("[inotify] dump event, wd: %d", ie->wd);

    if (ie->len > 0) {
        printf(", name: %s", ie->name);
    }

    printf("\n");

    if (ie->mask & IN_ACCESS) {
        printf("\tIN_ACCESS\n");
    }

    if (ie->mask & IN_OPEN) {
        printf("\tIN_OPEN\n");
    }

    if (ie->mask & IN_ATTRIB) {
        printf("\tIN_ATTRIB\n");
    }

    if (ie->mask & IN_CLOSE_WRITE) {
        printf("\tIN_CLOSE_WRITE\n");
    }

    if (ie->mask & IN_CLOSE_NOWRITE) {
        printf("\tIN_CLOSE_NOWRITE\n");
    }

    if (ie->mask & IN_MODIFY) {
        printf("\tIN_MODIFY\n");
    }

    if (ie->mask & IN_CREATE) {
        printf("\tIN_CREATE\n");
    }

    if (ie->mask & IN_DELETE) {
        printf("\tIN_DELETE\n");
    }

    if (ie->mask & IN_DELETE_SELF) {
        printf("\tIN_DELETE_SELF\n");
    }

    if (ie->mask & IN_MOVE_SELF) {
        printf("\tIN_MOVE_SELF\n");
    }

    if (ie->mask & IN_MOVED_FROM) {
        printf("\tIN_MOVE_FROM: cookie: %4d\n", ie->cookie);
    }

    if (ie->mask & IN_MOVED_TO) {
        printf("\tIN_MOVE_TO    cookie: %4d\n", ie->cookie);
    }

    if (ie->mask & IN_IGNORED) {
        printf("\tIN_IGNORED\n");
    }

    if (ie->mask & IN_Q_OVERFLOW) {
        printf("\tIN_Q_OVERFLOW\n");
    }
    if (ie->mask & IN_UNMOUNT) {
        printf("\tIN_UNMOUNT\n");
    }

    printf("\n");
}

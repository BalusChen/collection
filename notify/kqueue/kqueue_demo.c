
/*
 * Copyright (C) Jianyong Chen
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#define MAX_KEVENT      10


static int watch_file(int kq, char *path, int fflags);
static void dump_kqueue_event(struct kevent *ke);
static void dump_kevent(struct kevent *ke);


int
main(int argc, char **argv)
{
    int            i, n, kq, flags, fflags;
    struct kevent  kes[MAX_KEVENT];

    kq = kqueue();
    if (kq == -1) {
        perror("[kqueue] kqueue() failed");
        exit(EXIT_FAILURE);
    }

    flags = EV_ADD | EV_ENABLE | EV_CLEAR;
    fflags = NOTE_WRITE | NOTE_RENAME | NOTE_DELETE | NOTE_ATTRIB;

    for (i = 1; i < argc; i++) {
        printf("[kqueue] watch: %s\n", argv[i]);

        if (watch_file(kq, argv[i], fflags) == -1) {
            fprintf(stderr, "[kqueue] watch_file \"%s\" failed\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    for ( ;;  ) {
        n = kevent(kq, NULL, 0, kes, MAX_KEVENT, NULL);
        if (n == -1) {
            perror("kevent64() failed");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < n; i++) {
            dump_kevent(&kes[i]);
            dump_kqueue_event(&kes[i]);
        }
    }

    exit(EXIT_SUCCESS);
}


static int
watch_file(int kq, char *path, int flags)
{
    int            fd;
    struct kevent  ke;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    EV_SET(&ke, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, flags, 0, 0);

    return kevent(kq, &ke, 1, NULL, 0, NULL);
}


static void
dump_kqueue_event(struct kevent *ke)
{
    if (ke->fflags & NOTE_DELETE) {
        printf("\tdelete\n");
    }

    if (ke->fflags & NOTE_ATTRIB) {
        printf("\tattrib\n");

    }

    if (ke->fflags & NOTE_RENAME) {
        printf("\trename\n");
    }

    if (ke->fflags & NOTE_WRITE) {
        printf("\twrite\n");
    }
}

static void dump_kevent(struct kevent *ke)
{
    printf("id: %llu, filter: %d, flags: %d, fflags: %d, data: %ld, udata: %s\n",
            (unsigned long long) ke->ident, ke->filter, ke->flags, ke->fflags,
            ke->data, (char *) ke->udata);
}



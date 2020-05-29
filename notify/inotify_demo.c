
/*
 * Copyright (C) Jianyong Chen
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/epoll.h>


#define STOP                        "stop"
#define NEWLINE                     '\n'
#define MAX_LINE                    1024
#define MAX_FILE_PATHS              1024
#define MAX_EPOLL_EVENTS            10
#define INOTIFY_EVENTS_BUF_SIZE     (sizeof(struct inotify_event) * 10)


static int handle_stdin(int fd);
static int handle_inotify(int fd);

static int watch_single_path_recursively(int infd, char *path, int flags);
static int watch_single_path(int infd, char *path, int flags);

static void parse_options(int argc, char **argv);
static void watch_paths(int infd);
static void usage(FILE *fp);
static ssize_t readline(int fd, void *vptr, size_t maxlen);
static void dump_inotify_event(struct inotify_event *ie);


typedef struct {
    unsigned   verbose:1;
    unsigned   recursive:1;
    char      *path_file;
} options_t;


static options_t  cli_options;

static char *file_paths[MAX_FILE_PATHS];
static int   next_free_pos;


int
main(int argc, char **argv)
{
    int                 wd, i, nfds, ret, epfd, infd, flags;
    struct epoll_event  ee, events[MAX_EPOLL_EVENTS];

    parse_options(argc, argv);

    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("[inotify] epoll_create1() failed\n");
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

    watch_paths(infd);

    printf("[inotify] start to monitor, enter \"stop<enter>\" to leave...\n");

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
}


static void
parse_options(int argc, char **argv)
{
    int    len, ch, wd;
    char   *p, *path, buf[NAME_MAX];
    FILE  *fp;

    while ((ch = getopt(argc, argv, "?hvrf:p:")) != -1) {

        switch (ch) {
        case 'v':
            cli_options.verbose = 1;
            break;

        case 'r':
            cli_options.recursive = 1;
            break;

        case '?':
        case 'h':
            usage(stdout);
            exit(EXIT_SUCCESS);

        case 'p':
            if (next_free_pos == MAX_FILE_PATHS) {
                fprintf(stderr, "[inotify] too many paths to watch,"
                                " upper limit: %d", MAX_FILE_PATHS);
                exit(EXIT_FAILURE);
            }

            len = strlen(optarg);
            p = malloc(len + 1);
            if (p == NULL) {
                perror("malloc() error");
                exit(EXIT_FAILURE);
            }

            strncpy(p, optarg, len);
            p[len] = 0;
            file_paths[next_free_pos++] = p;
            break;

        case 'f':
            cli_options.path_file = optarg;
            break;

        default:
            usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    if (cli_options.path_file != NULL) {
        fp = fopen(cli_options.path_file, "r");

        if (fp == NULL) {
            fprintf(stderr, "[inotify] open \"%s\" error: %s",
                    cli_options.path_file, strerror(errno));

            exit(EXIT_FAILURE);
        }

        while ((path = fgets(buf, NAME_MAX, fp)) != NULL) {

            if (next_free_pos == MAX_FILE_PATHS) {
                fprintf(stderr, "[inotify] too many file to watch,"
                                " upper limit: %d\n", MAX_FILE_PATHS);

                exit(EXIT_FAILURE);
            }

            len = strlen(path) - 1; // minus newline
            p = malloc(len + 1);    // plus zero byte
            if (p == NULL) {
                perror("malloc() error");
                exit(EXIT_FAILURE);
            }

            strncpy(p, path, len);
            p[len] = 0;

            file_paths[next_free_pos++] = p;
        }
    }
}


static void
watch_paths(int infd)
{
    int     ret, i, wd, flags;

    if (cli_options.verbose && cli_options.recursive) {
        printf("[inotify] since \"-r\" option is specified, it may take a while"
               " to watch all paths recursively...\n");
    }

    // FIXME: specify events in cli

    // flags = IN_CREATE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_DELETE_SELF;
    flags = IN_ALL_EVENTS;

    for (i = 0; i < next_free_pos; i++) {

        if (cli_options.recursive) {
            ret = watch_single_path_recursively(infd, file_paths[i], flags);

        } else {
            ret = watch_single_path(infd, file_paths[i], flags);
        }

        if (ret == -1) {
            exit(EXIT_FAILURE);
        }
    }
}


static int
watch_single_path_recursively(int infd, char *path, int flags)
{
    int             len;
    DIR            *dir;
    char            buf[NAME_MAX];
    struct dirent  *entry;

    dir = opendir(path);

    if (dir == NULL) {

        // not directory, just watch it as a normal file.
        if (errno == ENOTDIR) {
            return watch_single_path(infd, path, flags);
        }

        return -1;
    }

    /* watch each item in this directory */

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            if (cli_options.verbose) {
                printf("[inotify] meet dir itself or parent dir of \"%s\" "
                       "skip...\n", path);
            }

            continue;
        }

        sprintf(buf, "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {

            if (watch_single_path_recursively(infd, buf, flags) == -1)
            {
                return -1;
            }

        } else if (entry->d_type == DT_REG) {

            if (watch_single_path(infd, buf, flags) == -1) {
                return -1;
            }

        } else {

            if (cli_options.verbose) {
                printf("[inotify] meet %s, not regular file or directory, "
                       "skip...\n", buf);
            }
        }
    }

    (void) closedir(dir);

    if (watch_single_path(infd, path, flags) == -1) {
        return -1;
    }

    return 0;
}


static int
watch_single_path(int infd, char *path, int flags)
{
    int     wd;

    if (cli_options.verbose) {
        printf("[inotify] watch \"%s\"\n", path);
    }

    wd = inotify_add_watch(infd, path, flags);
    if (wd == -1) {
        fprintf(stderr, "[inotify] watch \"%s\" error: %s\n", path,
                strerror(errno));

        return -1;
    }
}


static int
handle_stdin(int fd)
{
    int   n;
    char  buf[MAX_LINE];

    n = readline(fd, buf, MAX_LINE);
    if (n == sizeof(STOP) - 1 && strncmp(buf, STOP, sizeof(STOP) - 1) == 0) {
        printf("[inotify] receive stop directive, bye bye...\n");
        exit(EXIT_SUCCESS);

    } else if (n == -1) {
        perror("[inotify] read from stdin error");
        exit(EXIT_FAILURE);
    }

    printf("[inotify] unknown directive \"%s\" from stdin, skip...\n", buf);

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


static ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
    char     c, *ptr;
    ssize_t  n, rc;

    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
again:
        rc = read(fd, &c, 1);

        if (rc == 1) {

            if (c == NEWLINE) {
                *ptr = 0;
                return n - 1;
            }

            *ptr++ = c;

        } else if (rc == 0) {
            *ptr = 0;
            return n - 1;

        } else {

            if (errno == EINTR) {
                goto again;
            }

            return -1;
        }
    }

    *ptr = 0;
    return n;
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


static void
usage(FILE *fp)
{
    fprintf(fp, "\nusage ./inotify [-hvr] -p path -f file\n"
            "\t-h:    print this help and exit\n"
            "\t-v:    print verbose output\n"
            "\t-r:    recursively watch directory\n"
            "\t-f:    specify file which contains multiple paths to watch\n"
            "\t-p:    specify single path to watch\n");
}


/*
 * Copyright (C) Jianyong Chen
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/epoll.h>

#include "../rbtree/rb_tree.h"


#define STOP                        "stop"
#define NEWLINE                     '\n'
#define MAX_LINE                    1024
#define MAX_PATH                    1024
#define MAX_FILE_PATHS              1024
#define MAX_EPOLL_EVENTS            10
#define INOTIFY_EVENTS_BUF_SIZE     (sizeof(struct inotify_event) * 10)


typedef struct {
    rbtree_node_t   node;
    char            path[MAX_PATH];
} inotify_wd_node_t;


typedef struct {
    int             inotify_fd;

    /* cli options */

    unsigned        verbose:1;
    unsigned        recursive:1;
    char           *path_file;

    /* map wd to path */

    rbtree_t        wd_tree;
    rbtree_node_t   wd_sentinel;

    /* temporary array to store root paths specified in cli */

    char           *file_paths[MAX_FILE_PATHS];
    int             next_free_pos;
} inotify_demo_ctx_t;


static void inotify_parse_options(inotify_demo_ctx_t *ctx, int argc,
    char **argv);
static void inotify_init_ctx(inotify_demo_ctx_t  *ctx);
static inotify_wd_node_t *inotify_rbtree_lookup(inotify_demo_ctx_t *ctx,
    int wd);

static void inotify_watch_paths(inotify_demo_ctx_t *ctx);
static int inotify_watch_single_path_recursively(inotify_demo_ctx_t *ctx,
    char *path, int flags);
static int inotify_watch_single_path(inotify_demo_ctx_t *ctx, char *path,
    int flags);

static int handle_stdin(inotify_demo_ctx_t *ctx);
static int handle_inotify(inotify_demo_ctx_t *ctx);
static ssize_t readline(int fd, void *vptr, size_t maxlen);
static void inotify_usage(FILE *fp);
static void inotify_dump_event(inotify_demo_ctx_t *ctx,
    struct inotify_event *ie);


int
main(int argc, char **argv)
{
    int                 i, nfds, epfd, infd;
    inotify_demo_ctx_t  ctx;
    struct epoll_event  ee, events[MAX_EPOLL_EVENTS];

    inotify_init_ctx(&ctx);

    inotify_parse_options(&ctx, argc, argv);

    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("[inotify] epoll_create1(0) failed");
        exit(EXIT_FAILURE);
    }

    infd = inotify_init1(0);
    if (infd == -1) {
        perror("[inotify] inotify_init1(0) failed");
        exit(EXIT_FAILURE);
    }

    ctx.inotify_fd = infd;

    ee.events = EPOLLIN;
    ee.data.fd = STDIN_FILENO;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ee) < 0) {
        perror("[inotify] epoll_ctl(ADD, stdin) failed");
        exit(EXIT_FAILURE);
    }

    ee.events = EPOLLIN;
    ee.data.fd = infd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, infd, &ee) < 0) {
        perror("[inotify] epoll_ctl(ADD, inotify_fd) failed");
        exit(EXIT_FAILURE);
    }

    inotify_watch_paths(&ctx);

    printf("[inotify] start to monitor, enter \"stop<enter>\" to leave...\n");

    for ( ;; ) {
        nfds = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, -1);
        if (nfds == -1) {
            perror("[inotify] epoll_wait() failed\n");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < nfds; i++) {

            if (events[i].data.fd == STDIN_FILENO) {
                handle_stdin(&ctx);

            } else if (events[i].data.fd == infd) {
                handle_inotify(&ctx);

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
inotify_init_ctx(inotify_demo_ctx_t *ctx)
{
    rbtree_init(&ctx->wd_tree, &ctx->wd_sentinel, rbtree_insert_value);

    ctx->verbose = 0;
    ctx->recursive = 0;
    memset(&ctx->file_paths, 0, sizeof(ctx->file_paths));
    ctx->next_free_pos = 0;
}


static inotify_wd_node_t *
inotify_rbtree_lookup(inotify_demo_ctx_t *ctx, int wd)
{
    rbtree_node_t      *node, *sentinel;
    inotify_wd_node_t  *wn;

    node = ctx->wd_tree.root;
    sentinel = &ctx->wd_sentinel;

    while (node != sentinel) {

        if (wd < node->key) {
            node = node->left;
            continue;
        }

        if (wd > node->key) {
            node = node->right;
            continue;
        }

        /* wd == node->key */

        wn = (inotify_wd_node_t *) node;
        return wn;
    }

    return NULL;
}


static void
inotify_parse_options(inotify_demo_ctx_t *ctx, int argc, char **argv)
{
    int    len, ch;
    char   *p, *path, buf[NAME_MAX];
    FILE  *fp;

    while ((ch = getopt(argc, argv, "?hvrf:p:")) != -1) {

        switch (ch) {
        case 'v':
            ctx->verbose = 1;
            break;

        case 'r':
            ctx->recursive = 1;
            break;

        case '?':
        case 'h':
            inotify_usage(stdout);
            exit(EXIT_SUCCESS);

        case 'p':
            if (ctx->next_free_pos == MAX_FILE_PATHS) {
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
            ctx->file_paths[ctx->next_free_pos++] = p;
            break;

        case 'f':
            ctx->path_file = optarg;
            break;

        default:
            inotify_usage(stderr);
            exit(EXIT_FAILURE);
        }
    }

    if (ctx->path_file != NULL) {
        fp = fopen(ctx->path_file, "r");

        if (fp == NULL) {
            fprintf(stderr, "[inotify] open \"%s\" error: %s",
                    ctx->path_file, strerror(errno));

            exit(EXIT_FAILURE);
        }

        while ((path = fgets(buf, NAME_MAX, fp)) != NULL) {

            if (ctx->next_free_pos == MAX_FILE_PATHS) {
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

            ctx->file_paths[ctx->next_free_pos++] = p;
        }
    }
}


static void
inotify_watch_paths(inotify_demo_ctx_t *ctx)
{
    int     ret, i, flags;

    if (ctx->verbose && ctx->recursive) {
        printf("[inotify] since \"-r\" option is specified, it may take a while"
               " to watch all paths recursively...\n");
    }

    // TODO: specify events in cli

    // flags = IN_CREATE | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_DELETE_SELF;
    flags = IN_ALL_EVENTS;

    for (i = 0; i < ctx->next_free_pos; i++) {

        if (ctx->recursive) {
            ret = inotify_watch_single_path_recursively(ctx,
                                                        ctx->file_paths[i],
                                                        flags);

        } else {
            ret = inotify_watch_single_path(ctx, ctx->file_paths[i], flags);
        }

        if (ret == -1) {
            exit(EXIT_FAILURE);
        }
    }
}


static int
inotify_watch_single_path_recursively(inotify_demo_ctx_t *ctx, char *path,
    int flags)
{
    DIR            *dir;
    char            buf[NAME_MAX];
    struct dirent  *entry;

    dir = opendir(path);

    if (dir == NULL) {

        // not directory, just watch it as a normal file.
        if (errno == ENOTDIR) {
            return inotify_watch_single_path(ctx, path, flags);
        }

        fprintf(stderr, "[inotify] open directory \"%s\" error: %s\n", path,
                strerror(errno));

        return -1;
    }

    /* watch each item in this directory */

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        sprintf(buf, "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {

            if (inotify_watch_single_path_recursively(ctx, buf, flags) == -1)
            {
                return -1;
            }

        } else if (entry->d_type == DT_REG) {

            if (inotify_watch_single_path(ctx, buf, flags) == -1) {
                return -1;
            }

        } else {

            if (ctx->verbose) {
                printf("[inotify] \"%s\" is not directory or regular file,"
                       " skip...\n", buf);
            }
        }
    }

    (void) closedir(dir);

    return inotify_watch_single_path(ctx, path, flags);
}


static int
inotify_watch_single_path(inotify_demo_ctx_t *ctx, char *path, int flags)
{
    int                 wd;
    inotify_wd_node_t  *wn;

    if (ctx->verbose) {
        printf("[inotify] watch \"%s\"\n", path);
    }

    wd = inotify_add_watch(ctx->inotify_fd, path, flags);
    if (wd == -1) {
        fprintf(stderr, "[inotify] watch \"%s\" error: %s\n", path,
                strerror(errno));
        return -1;
    }

    wn = malloc(sizeof(inotify_wd_node_t));
    if (wn == NULL) {
        return -1;
    }

    wn->node.key = wd;
    strcpy(wn->path, path);

    rbtree_insert(&ctx->wd_tree, &wn->node);

    return 0;
}


static int
handle_stdin(inotify_demo_ctx_t *ctx)
{
    int   n;
    char  buf[MAX_LINE];

    n = readline(STDIN_FILENO, buf, MAX_LINE);
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
handle_inotify(inotify_demo_ctx_t *ctx)
{
    int                    n;
    char                  *p;
    struct inotify_event  *ie;

    char buf[INOTIFY_EVENTS_BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    n = read(ctx->inotify_fd, buf, sizeof(buf));
    if (n == -1) {
        perror("[inotify] read from inotify fd failed");
        exit(EXIT_FAILURE);
    }

    for (p = buf; p + sizeof(struct inotify_event) <= buf + n; /* void */) {
        ie = (struct inotify_event *) p;

        inotify_dump_event(ctx, ie);

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
inotify_dump_event(inotify_demo_ctx_t *ctx, struct inotify_event *ie)
{
    inotify_wd_node_t  *wn;

    printf("[inotify] dump event, wd: %d", ie->wd);

    wn = inotify_rbtree_lookup(ctx, ie->wd);
    if (wn != NULL) {
        printf(", rb_path: %s", wn->path);
    }

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
inotify_usage(FILE *fp)
{
    fprintf(fp, "\nusage ./inotify [-hvr] -p path -f file\n"
            "\t-h:    print this help and exit\n"
            "\t-v:    print verbose output\n"
            "\t-r:    recursively watch directory\n"
            "\t-f:    specify file which contains multiple paths to watch\n"
            "\t-p:    specify single path to watch\n");
}

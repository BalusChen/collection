
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

#include <rb_tree.h>


#define STOP                        "stop"
#define NEWLINE                     '\n'

/* FIXME: use macros of posix standard */

#define MAX_LINE                    1024
#define MAX_NAME                    1024
#define MAX_PATH                    1024
#define MAX_FILE_PATHS              1024
#define MAX_EPOLL_EVENTS            10
#define INOTIFY_EVENTS_BUF_SIZE     (sizeof(struct inotify_event) * 10)

typedef struct {
    char    old_prefix[1024];
    char    new_prefix[1024];
} inotify_rbtree_rename_data_t;


typedef struct {
    rbtree_node_t   node;
    char            path[MAX_PATH];
} inotify_wd_node_t;


typedef struct {
    int                    inotify_fd;

    /* cli options */

    unsigned               verbose:1;
    unsigned               recursive:1;
    char                  *path_file;

    /* map wd to path */

    rbtree_t               wd_tree;
    rbtree_node_t          wd_sentinel;

    struct inotify_event  *last_event;
    /* temporary array to store root paths specified in cli */

    char                  *file_paths[MAX_FILE_PATHS];
    int                    next_free_pos;
} inotify_demo_ctx_t;


static int inotify_init_ctx(inotify_demo_ctx_t  *ctx);
static void inotify_parse_options(inotify_demo_ctx_t *ctx, int argc,
    char **argv);

static inotify_wd_node_t *inotify_rbtree_lookup(inotify_demo_ctx_t *ctx,
    int wd);
static void inotify_rbtree_rename_prefix(rbtree_node_t *node, void *arg);

static void inotify_watch_paths(inotify_demo_ctx_t *ctx);
static int inotify_watch_single_path_recursively(inotify_demo_ctx_t *ctx,
    char *path, int flags);
static int inotify_watch_single_path(inotify_demo_ctx_t *ctx, char *path,
    int flags);

static int handle_stdin(inotify_demo_ctx_t *ctx);
static int handle_inotify(inotify_demo_ctx_t *ctx);
static ssize_t readline(int fd, void *vptr, size_t maxlen);
static void inotify_usage(FILE *fp);

static void inotify_process_event(inotify_demo_ctx_t *ctx,
    struct inotify_event *ie);
static void inotify_dump_event(inotify_wd_node_t *wn,
    struct inotify_event *ie);


int
main(int argc, char **argv)
{
    int                 i, nfds, epfd, infd;
    inotify_demo_ctx_t  ctx;
    struct epoll_event  ee, events[MAX_EPOLL_EVENTS];

    if (inotify_init_ctx(&ctx) == -1) {
        exit(EXIT_FAILURE);
    }

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


static int
inotify_init_ctx(inotify_demo_ctx_t *ctx)
{
    rbtree_init(&ctx->wd_tree, &ctx->wd_sentinel, rbtree_insert_value);

    ctx->verbose = 0;
    ctx->recursive = 0;

    memset(&ctx->file_paths, 0, sizeof(ctx->file_paths));
    ctx->next_free_pos = 0;

    ctx->last_event = calloc(1, sizeof(struct inotify_event) + MAX_NAME);
    if (ctx->last_event == NULL) {
        perror("calloc() error");
        return -1;
    }

    return 0;
}


static void
inotify_parse_options(inotify_demo_ctx_t *ctx, int argc, char **argv)
{
    int    len, ch;
    char   *p, *path, buf[MAX_NAME];
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
            // FIXME: Don't Repeat Yourself.

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

        while ((path = fgets(buf, MAX_NAME, fp)) != NULL) {

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
    char            buf[MAX_NAME];
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

        inotify_process_event(ctx, ie);

        memcpy(ctx->last_event, ie, sizeof(struct inotify_event) + ie->len);
        ctx->last_event->name[ie->len] = 0;

        p += (sizeof(struct inotify_event) + ie->len);
    }

    return 0;
}


static void
inotify_process_event(inotify_demo_ctx_t *ctx, struct inotify_event *ie)
{
    char                           buf[1024];
    inotify_wd_node_t             *wn, *own;
    inotify_rbtree_rename_data_t   data;

    wn = inotify_rbtree_lookup(ctx, ie->wd);

    if (wn == NULL) {
        return;
    }

    // For debug
    inotify_dump_event(wn, ie);

    /*
     * NOTE: 如果是 DELETE 事件，说明是监听的目录下面的子目录/文件被删除产生的事件，
     *       而只有在 recursive 模式，才会递归监听，然而，只要监听了，那么就还会产生
     *       DELETE_SELF 事件。
     *       而对于非 recursive 模式，由于并没有将其添加到 inotify 和红黑树，所以也
     *       不用将其从中移除；所以实际上只需要处理 DELETE_SELF 事件即可。
     *
     * NOTE: recursive 模式下，删除了某个父目录，除了该父目录本身外，其下被监听的子
     *       目录/文件也会发出 DELETE_SELF 事件（其后还跟着一个 IGNORED 事件），
     *       所以就算是 recursive 模式，也不用递归删除。
     */

    if ((ie->mask & IN_DELETE) && ctx->recursive) {
        // TODO: process delete event
    }

    /*
     * If we last had a MOVED_FROM event but without a MOVED_TO event currently,
     * the path must have been moved out of tree, so just unwatch it.
     *
     */

    /*
     * FIXME: These two events are usually consecutive in the event
     *        stream available when reading from the inotify file descriptor.
     *        However, this is not guaranteed.
     */

    if (((ctx->last_event->mask & IN_MOVED_FROM) && !(ie->mask & IN_MOVED_TO))
        || (ie->mask & IN_DELETE_SELF))
    {
        if (ctx->verbose) {
            printf("[inotify] unwatch \"%s\" since it has been"
                   " removed from tree\n", wn->path);
        }

        if (inotify_rm_watch(ctx->inotify_fd, ie->wd) == -1) {
            printf("[inotify] unwatch \"%s\" failed: %s\n", wn->path,
                   strerror(errno));
        }

        rbtree_delete(&ctx->wd_tree, &wn->node);
        free(wn);
    }

    /*
     * When rename event occurred and the recursive flag has been set,
     * we must replace old directory prefix with the new one for all
     * watched items within this directory.
     */

    if ((ctx->last_event->mask & IN_MOVED_FROM)
        && (ie->mask && IN_MOVED_TO)
        && (ie->cookie == ctx->last_event->cookie))
    {
        own = inotify_rbtree_lookup(ctx, ctx->last_event->wd);

        if (own != NULL) {

            // For debug
            printf("[inotify] o_name: %s, o_path: %s, n_name: %s,"
                   " n_path: %s\n", ctx->last_event->name, own->path,
                   ie->name, wn->path);

            sprintf(data.old_prefix, "%s/%s", own->path, ctx->last_event->name);
            sprintf(data.new_prefix, "%s/%s", wn->path, ie->name);

            if (ctx->verbose) {
                printf("[inotify] process rename event: old_prefix: %s,"
                       " new_prefix: %s\n", data.old_prefix, data.new_prefix);
            }

            rbtree_traverse(&ctx->wd_tree, inotify_rbtree_rename_prefix, &data);
        }
    }

    /*
     * File moved from unwatched tree or newly created, we treat both
     * cases as new file creation, and watch it in recursive mode.
     */

    if (ctx->recursive
        && ((!(ctx->last_event->mask & IN_MOVED_FROM)
             && (ie->mask & IN_MOVED_TO))
            || (ie->mask & IN_CREATE)))
    {
        sprintf(buf, "%s/%s", wn->path, ie->name);
        inotify_watch_single_path_recursively(ctx, buf, IN_ALL_EVENTS);
    }
}


static void
inotify_dump_event(inotify_wd_node_t *wn, struct inotify_event *ie)
{
    printf("[inotify] dump event, wd: %d", ie->wd);

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
inotify_rbtree_rename_prefix(rbtree_node_t *node, void *arg)
{
    int                            len, plen;
    char                          buf[1024];
    inotify_wd_node_t             *wn;
    inotify_rbtree_rename_data_t  *data;

    wn = (inotify_wd_node_t *) node;
    data = (inotify_rbtree_rename_data_t *) arg;

    len = strlen(wn->path);
    plen = strlen(data->old_prefix);

    if (len >= plen && strncmp(data->old_prefix, wn->path, plen) == 0) {
        printf("[inotify] rename from \"%s\" to ", wn->path);

        strncpy(buf, &wn->path[plen], len - plen + 1);
        sprintf(wn->path, "%s%s", data->new_prefix, buf);

        printf("\"%s\"\n", wn->path);
    }
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
inotify_usage(FILE *fp)
{
    fprintf(fp, "\nusage ./inotify [-hvr] -p path -f file\n"
            "\t-h:    print this help and exit\n"
            "\t-v:    print verbose output\n"
            "\t-r:    recursively watch directory\n"
            "\t-f:    specify file which contains multiple paths to watch\n"
            "\t-p:    specify single path to watch\n");
}

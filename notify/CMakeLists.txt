CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

IF (OS STREQUAL "Linux")

    MESSAGE(STATUS "[bulus] on Linux, compile inotify and eventfd demos")

    ADD_EXECUTABLE(inotify
            inotify/inotify_demo.c
            ../rbtree/rb_tree.c)

    ADD_EXECUTABLE(eventfd
            event_notify/eventfd_demo.c)

ELSEIF(OS STREQUAL "macOS")

    MESSAGE(STATUS "[balus] on macOS, compile kqueue demo")

    ADD_EXECUTABLE(kqueue
            kqueue/kqueue_demo.c)

ENDIF()

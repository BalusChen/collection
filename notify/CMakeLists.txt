CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

MESSAGE(STATUS "[balus] build notify demos, including inotify, kqueue, eventfd")
MESSAGE(STATUS "[balus] notify project directory: ${PROJECT_BINARY_DIR}")
MESSAGE(STATUS "[balus] notify source directory: ${PROJECT_SOURCE_DIR}")

IF(UNIX AND NOT APPLE)

    MESSAGE(STATUS "[bulus] on Linux, compile inotify and eventfd demos")

    ADD_EXECUTABLE(inotify
            inotify/inotify_demo.c ../rbtree/rb_tree.c)

    ADD_EXECUTABLE(eventfd
            event_notify/eventfd_demo.c)

ELSEIF(APPLE)

    MESSAGE(STATUS "[balus] on macOS, compile kqueue demo")

    ADD_EXECUTABLE(kqueue
            kqueue/kqueue_demo.c)

ENDIF()

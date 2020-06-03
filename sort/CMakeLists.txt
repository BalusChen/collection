CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

MESSAGE(STATUS "[balus] compiler sort demo")

ADD_EXECUTABLE(quick_sort_demo
        quick/quick_sort_test.cc quick/quick_sort.cc)

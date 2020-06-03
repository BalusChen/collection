cmake_minimum_required(VERSION 3.7)
project(collection)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

INCLUDE_DIRECTORIES(include)

ADD_SUBDIRECTORY(notify bin/notify)
ADD_SUBDIRECTORY(sort bin/sort)
ADD_SUBDIRECTORY(bloom-filter bin/bloom_filter)


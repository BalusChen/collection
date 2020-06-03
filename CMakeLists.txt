CMAKE_MINIMUM_REQUIRED(VERSION 3.7)
PROJECT(collection)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_C_STANDARD 11)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    SET(OS "macOS")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(OS "Linux")
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    SET(OS "Windows")
ELSE()
    SET(OS "Others")
ENDIF()

MESSAGE(STATUS "[balus] Binary directory: ${PROJECT_BINARY_DIR}")
MESSAGE(STATUS "[balus] Source directory: ${PROJECT_SOURCE_DIR}")
MESSAGE(STATUS "[balus] Compile project on ${OS}")

INCLUDE_DIRECTORIES(include)

ADD_SUBDIRECTORY(notify bin/notify)
ADD_SUBDIRECTORY(sort bin/sort)
ADD_SUBDIRECTORY(bloom-filter bin/bloom_filter)

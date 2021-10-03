cmake_minimum_required(VERSION 2.8)

project(logd)

set(CMAKE_C_FLAGS "-g -fPIC -Wall")

include_directories(.)
add_executable(${PROJECT_NAME} daemon.c misc.c array.c pubsub.c)
target_link_libraries(${PROJECT_NAME} pthread)

add_executable(logcat cat.c misc.c)
target_link_libraries(logcat)

add_library(log log.c misc.c)
target_link_libraries(log)

add_executable(logtest test.c)
target_link_libraries(logtest log)

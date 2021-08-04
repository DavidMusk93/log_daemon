cmake_minimum_required(VERSION 2.8)

project(logdaemon)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -fPIC -Wall")

set(BASE "base")
set(BASE_PATH "./3rdparty/c-playground/base")
aux_source_directory(${BASE_PATH} BASE_SRCS)
add_library(${BASE} STATIC ${BASE_SRCS})

include_directories(${BASE_PATH})
add_executable(${PROJECT_NAME} daemon.cpp)
target_link_libraries(${PROJECT_NAME} pthread ${BASE})

add_executable(logcat cat.cpp)
target_link_libraries(logcat ${BASE})

add_library(log log.cpp)
target_link_libraries(log ${BASE})

add_executable(logtest msg.cpp)
target_link_libraries(logtest log ${BASE})
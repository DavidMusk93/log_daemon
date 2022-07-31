cmake_minimum_required(VERSION 3.10)

project(logd)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

set(CMAKE_C_FLAGS "-g -fPIC -Wall -Werror")
set(EXTERN_LIBS "")

string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)
if (uppercase_CMAKE_BUILD_TYPE MATCHES "DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize=leak")
    set(EXTERN_LIBS "asan")
endif ()

#add_definitions(-D_DEBUG=1)

include_directories(.)
add_executable(${PROJECT_NAME} daemon.c misc.c array.c pubsub.c object.c filter.c packer.c)
target_link_libraries(${PROJECT_NAME} pthread ${EXTERN_LIBS})

add_executable(logcat cat.c misc.c packer.c filter.c object.c array.c)
target_link_libraries(logcat)

add_library(log log.c misc.c)
target_link_libraries(log)

add_executable(logtest test.c)
target_link_libraries(logtest log)

enable_testing()

add_executable(queue_test queue_test.c queue.h)
add_test(NAME queue_test COMMAND queue_test)

add_executable(ring_test ring_test.c array.c)
add_test(NAME ring_test COMMAND ring_test)

add_executable(filter_test filter.c array.c object.c)
target_compile_definitions(filter_test PRIVATE TEST_FILTER=1)
target_link_libraries(filter_test asan)
add_test(NAME filter_test COMMAND filter_test)

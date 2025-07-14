cmake_minimum_required(VERSION 3.30)
project(tasksys CXX)

set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -O0 -g")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -O0 -g")
if (TSAN)
    set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=thread")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=thread")
endif ()
if (ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif ()

set(TASKSYS_BUILD_TYPE SHARED CACHE STRING "Library type of tasksys (SHARED|STATIC)")

add_subdirectory(test)

file(GLOB_RECURSE SOURCES lib/*)
file(GLOB_RECURSE HEADERS include/*)

add_library(tasksys ${TASKSYS_BUILD_TYPE} ${SOURCES} ${HEADERS})
target_include_directories(tasksys PUBLIC include)
target_compile_features(tasksys PUBLIC cxx_std_23)

message(STATUS "sizeof(void*) = ${CMAKE_SIZEOF_VOID_P}")
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # for `cmpxchg16b`
    target_compile_options(tasksys PUBLIC -mcx16)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(tasksys PRIVATE -D_DEBUG)
endif ()

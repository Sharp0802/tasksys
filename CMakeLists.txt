cmake_minimum_required(VERSION 3.30)
project(tasksys CXX)


if (PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif ()


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}    -O0 -g")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -O0 -g")
if (TSAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=thread")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=thread")
endif ()
if (ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}    -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif ()


set(TASKSYS_BUILD_TYPE SHARED CACHE STRING "Library type of tasksys (SHARED|STATIC)")

file(GLOB_RECURSE SOURCES lib/*)
file(GLOB_RECURSE HEADERS include/*)

add_library(tasksys ${TASKSYS_BUILD_TYPE} ${SOURCES} ${HEADERS})
target_include_directories(tasksys PUBLIC include)
target_compile_features(tasksys PUBLIC cxx_std_23)


if (PROJECT_IS_TOP_LEVEL)
    enable_testing()

    add_executable(tasksys.test ${SOURCES} ${HEADERS})
    target_include_directories(tasksys.test PUBLIC include)
    target_compile_features(tasksys.test PUBLIC cxx_std_23)
    target_link_libraries(tasksys.test GTest::gtest_main)
    target_compile_definitions(tasksys.test PRIVATE __TS_TEST__)

    include(GoogleTest)
    gtest_discover_tests(tasksys.test)
endif ()

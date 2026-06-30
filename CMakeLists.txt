cmake_minimum_required(VERSION 3.28.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER /usr/bin/gcc)
set(CMAKE_CXX_COMPILER /usr/bin/g++)

project(Camus VERSION 4.0.0 LANGUAGES CXX DESCRIPTION "Static blog generation tool developed by C++")

enable_testing()

find_package(Git QUIET)
if (GIT_FOUND)
    # 获取完整 40 位 hash
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            OUTPUT_VARIABLE GIT_COMMIT_LONG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # 获取分支名
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # 检查工作区是否干净
    execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --quiet
            RESULT_VARIABLE GIT_DIFF_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --cached --quiet
            RESULT_VARIABLE GIT_CACHED_RESULT
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if (GIT_DIFF_RESULT EQUAL 0 AND GIT_CACHED_RESULT EQUAL 0)
        set(GIT_IS_CLEAN "true")
    else ()
        set(GIT_IS_CLEAN "false")
    endif ()
else ()
    set(GIT_COMMIT_LONG "unknown")
    set(GIT_BRANCH "unknown")
    set(GIT_IS_CLEAN "false")
endif ()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")
add_compile_definitions(
        BUILD_TIMESTAMP="${BUILD_TIME}"
        PROJECT_VERSION_MAJOR="${CMAKE_PROJECT_VERSION_MAJOR}"
        PROJECT_VERSION="${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${CMAKE_PROJECT_VERSION_PATCH}"
        GIT_BRANCH="${GIT_BRANCH}"
        GIT_REPO="https://github.com/o8x/camus"
        GIT_COMMIT_LONG="${GIT_COMMIT_LONG}"
        GIT_IS_CLEAN="${GIT_IS_CLEAN}"
        BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        CMAKE_VERSION="${CMAKE_VERSION}"
        CXX_STANDARD="${CMAKE_CXX_STANDARD}"
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -ffast-math -falign-functions=64 -flto=auto)
    add_link_options(-ffunction-sections)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_compile_options(-fuse-ld=mold -fno-semantic-interposition -mavx512f -mavx512cd -mavx512bw -mavx512dq)
        add_link_options(-Wl,--hash-style=gnu,--as-needed,--gc-sections,--print-gc-sections)
    endif()
endif ()

add_subdirectory(camus)
add_subdirectory(common)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(third-party/yaml-cpp EXCLUDE_FROM_ALL)
add_subdirectory(third-party/cmark-gfm EXCLUDE_FROM_ALL)

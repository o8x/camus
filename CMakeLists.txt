cmake_minimum_required(VERSION 3.28.6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Camus VERSION 0.1.0 LANGUAGES CXX)

add_subdirectory(camus)
add_subdirectory(common)
add_subdirectory(third-party/cmark-gfm EXCLUDE_FROM_ALL)

cmake_minimum_required(VERSION 3.28.6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Camus VERSION 0.1.0 LANGUAGES CXX)

file(GLOB SOURCE_FILES
        camus/*.h
        camus/*.cpp
        third-party/cmdline/*.h
)

add_library(camus STATIC ${SOURCE_FILES})
target_include_directories(camus PUBLIC camus)
target_include_directories(camus PUBLIC src third-party/cmdline)
target_link_libraries(camus PUBLIC maddy)

add_subdirectory(third-party/maddy)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE camus)

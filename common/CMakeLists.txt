file(GLOB SOURCES
        common/cmdline/*.h
        common/markdown/*.h
        common/markdown/*.cpp
        common/logging/*.h
        common/logging/*.cpp
        common/functions/*.h
        common/functions/*.cpp
        common/filesystem/*.h
        common/filesystem/*.cpp
)

add_library(common STATIC ${SOURCES})

target_include_directories(common PUBLIC
        .
        ${CMAKE_SOURCE_DIR}/third-party/cmark-gfm/src
        ${CMAKE_BINARY_DIR}/third-party/cmark-gfm/src
)

target_link_libraries(common PUBLIC
        libcmark-gfm_static
        yaml-cpp::yaml-cpp
)

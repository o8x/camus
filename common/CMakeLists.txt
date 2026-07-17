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
        common/str/*.h
        common/str/*.cpp
        common/extract/*.h
        common/extract/*.cpp
        common/net/*.h
        common/net/*.cpp
        ../third-party/cpp-httplib/httplib.h
)

add_library(common STATIC ${SOURCES})

find_package(PkgConfig REQUIRED)

if (APPLE)
    set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/opt/libarchive/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif ()

pkg_check_modules(ARCHIVE REQUIRED libarchive)
target_link_libraries(common PUBLIC ${ARCHIVE_LIBRARIES})
target_include_directories(common PUBLIC ${ARCHIVE_INCLUDE_DIRS})

target_include_directories(common PUBLIC
        .
        ${PROJECT_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/third-party/cmark-gfm/src
        ${CMAKE_BINARY_DIR}/third-party/cmark-gfm/src
        ../third-party/cpp-httplib
)

target_link_libraries(common PUBLIC
        libcmark-gfm_static
        yaml-cpp::yaml-cpp
        nlohmann_json
        inja
)

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/include/build/build.h")

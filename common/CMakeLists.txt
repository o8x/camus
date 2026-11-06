file(GLOB SOURCES
        common/cmdline/*.h
        common/render/*.h
        common/render/*.cpp
        common/logging/*.h
        common/logging/*.cpp
        common/functions/*.h
        common/functions/*.cpp
        common/filesystem/*.h
        common/filesystem/*.cpp
        common/str/*.h
        common/str/*.cpp
        common/paths/*.h
        common/paths/*.cpp
        common/net/*.h
        common/net/*.cpp
        ../third-party/cpp-httplib/httplib.h
)

add_library(common STATIC ${SOURCES})

target_include_directories(common PUBLIC
        .
        ${PROJECT_BINARY_DIR}/include
)

target_include_directories(common SYSTEM PUBLIC
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

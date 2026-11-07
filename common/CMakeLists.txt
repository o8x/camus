find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)

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
        PkgConfig::OPENSSL
)

# cpp-httplib 客户端/服务端支持 HTTPS（--update 请求 GitHub API）。
target_compile_definitions(common PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

if (APPLE)
    # cpp-httplib 在 macOS 上通过 Security / CoreFoundation 读取系统根证书。
    target_link_libraries(common PUBLIC "-framework Security" "-framework CoreFoundation")
endif()

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/include/build/build.h")

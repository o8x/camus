file(GLOB SOURCE_FILES
        src/*.h
        src/*.cpp
)

add_library(camus STATIC ${SOURCE_FILES})
target_link_libraries(camus PUBLIC common)
target_include_directories(camus PUBLIC src)

add_executable(Camus main.cpp)
target_link_libraries(Camus PRIVATE camus)
set_target_properties(Camus PROPERTIES OUTPUT_NAME "camus" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(DEFAULT_SITE "site")

file(WRITE ${CMAKE_BINARY_DIR}/default-site.txt "${DEFAULT_SITE}")
add_test(NAME build-test-site
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} "${CMAKE_BINARY_DIR}/bin/camus" -d ${DEFAULT_SITE}
)

# 切换工作目录 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
set_tests_properties(build-test-site PROPERTIES TIMEOUT 10)

file(COPY_FILE "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/LICENSE.txt")
file(COPY_FILE "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/README.txt")
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${CMAKE_PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "https://github.com/the-writer")
set(CPACK_PACKAGE_CONTACT "https://github.com/the-writer/camus/issues")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/README.txt")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")

cmake_host_system_information(RESULT HOST_OS_NAME QUERY OS_NAME)
cmake_host_system_information(RESULT HOST_OS_RELEASE QUERY OS_RELEASE)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-v${CPACK_PACKAGE_VERSION}_${HOST_OS_NAME}${HOST_OS_RELEASE}-${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER ${CPACK_PACKAGE_FILE_NAME} CPACK_PACKAGE_FILE_NAME)

if (APPLE)
    # macOS 默认安装到用户目录 ~/Library/Application Support/Camus；
    # 选择系统域（如 installer -target /）时落到 /Library/Application Support/Camus，运行期作为回退路径。
    set(CAMUS_RESOURCE_DIR "Library/Application Support/Camus")

    if (CPACK_GENERATOR MATCHES "DragNDrop")
        set_target_properties(Camus PROPERTIES
                MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
                MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
                MACOSX_BUNDLE TRUE
        )

        install(TARGETS Camus BUNDLE DESTINATION ".")
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/example/theme" "${CMAKE_SOURCE_DIR}/example/assets"
                "${CMAKE_SOURCE_DIR}/example/posts"
                DESTINATION "${CAMUS_RESOURCE_DIR}")
        install(FILES "${CMAKE_SOURCE_DIR}/example/camus.yaml" "${CMAKE_SOURCE_DIR}/example/.gitignore" "${CMAKE_SOURCE_DIR}/example/Justfile"
                DESTINATION "${CAMUS_RESOURCE_DIR}")
    elseif (CPACK_GENERATOR MATCHES "productbuild")
        # pkgutil --pkgs
        # pkgutil --pkg-info cn.com.stdout.Camus.Runtime
        # 手动卸载
        # rm -f /usr/local/bin/Camus
        # pkgutil --forget cn.com.stdout.Camus.Runtime
        install(TARGETS Camus DESTINATION "./usr/local/bin" COMPONENT Runtime)
        install(DIRECTORY "${CMAKE_SOURCE_DIR}/example/theme" "${CMAKE_SOURCE_DIR}/example/assets"
                "${CMAKE_SOURCE_DIR}/example/posts"
                DESTINATION "${CAMUS_RESOURCE_DIR}" COMPONENT Resources)
        install(FILES "${CMAKE_SOURCE_DIR}/example/camus.yaml" "${CMAKE_SOURCE_DIR}/example/.gitignore" "${CMAKE_SOURCE_DIR}/example/Justfile"
                DESTINATION "${CAMUS_RESOURCE_DIR}" COMPONENT Resources)

        set(CPACK_COMPONENTS_ALL Runtime Resources)
        set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "${CMAKE_PROJECT_NAME}")
        set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "${CMAKE_PROJECT_NAME} executable")
        set(CPACK_COMPONENT_RESOURCES_DISPLAY_NAME "${CMAKE_PROJECT_NAME} resources")
        set(CPACK_COMPONENT_RESOURCES_DESCRIPTION "${CMAKE_PROJECT_NAME} themes, assets and example template")
        set(CPACK_MONOLITHIC_INSTALL OFF)
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
        set(CPACK_PRODUCTBUILD_IDENTIFIER "cn.com.stdout.Camus")
        set(CPACK_PRODUCTBUILD_DOMAINS ON)
        set(CPACK_PRODUCTBUILD_DOMAINS_USER ON)
        set(CPACK_PRODUCTBUILD_DOMAINS_ROOT ON)
    endif ()
elseif (UNIX)
    set(CAMUS_RESOURCE_DIR "var/lib/camus")

    install(TARGETS Camus DESTINATION ./usr/local/bin)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/example/theme" "${CMAKE_SOURCE_DIR}/example/assets"
            "${CMAKE_SOURCE_DIR}/example/posts"
            DESTINATION "${CAMUS_RESOURCE_DIR}")
    install(FILES "${CMAKE_SOURCE_DIR}/example/camus.yaml" "${CMAKE_SOURCE_DIR}/example/.gitignore" "${CMAKE_SOURCE_DIR}/example/Justfile"
                DESTINATION "${CAMUS_RESOURCE_DIR}")

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "https://github.com/the-writer")
    set(CPACK_MONOLITHIC_INSTALL ON)
endif ()

include(GNUInstallDirs)
include(CPack)

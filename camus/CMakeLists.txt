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

file(COPY_FILE "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/LICENSE.txt")
file(COPY_FILE "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/README.txt")
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${CMAKE_PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "https://github.com/o8x")
set(CPACK_PACKAGE_CONTACT "https://github.com/o8x/camus/issues")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_BINARY_DIR}/README.txt")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")

cmake_host_system_information(RESULT HOST_OS_NAME QUERY OS_NAME)
cmake_host_system_information(RESULT HOST_OS_RELEASE QUERY OS_RELEASE)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${HOST_OS_NAME}${HOST_OS_RELEASE}-${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER ${CPACK_PACKAGE_FILE_NAME} CPACK_PACKAGE_FILE_NAME)

if (APPLE)
    if(CPACK_GENERATOR MATCHES "DragNDrop")
        set_target_properties(Camus PROPERTIES
                MACOSX_BUNDLE_BUNDLE_VERSION ${CMAKE_PROJECT_VERSION}
                MACOSX_BUNDLE_SHORT_VERSION_STRING ${CMAKE_PROJECT_VERSION}
                MACOSX_BUNDLE TRUE
        )

        install(TARGETS Camus BUNDLE DESTINATION ".")
    elseif (CPACK_GENERATOR MATCHES "productbuild")
        # pkgutil --pkgs
        # pkgutil --pkg-info cn.com.stdout.Camus.Runtime
        # 手动卸载
        # rm -f /usr/local/bin/Camus
        # pkgutil --forget cn.com.stdout.Camus.Runtime
        install(TARGETS Camus DESTINATION "./usr/local/bin" COMPONENT Runtime)

        set(CPACK_COMPONENTS_ALL Runtime)
        set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "${CMAKE_PROJECT_NAME}")
        set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "${CMAKE_PROJECT_NAME} executable")
        set(CPACK_MONOLITHIC_INSTALL OFF)
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
        set(CPACK_PACKAGING_INSTALL_PREFIX "/")
        set(CPACK_PRODUCTBUILD_IDENTIFIER "cn.com.stdout.Camus")
    endif()
elseif (UNIX)
    install(TARGETS Camus DESTINATION ./usr/local/bin)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "https://github.com/o8x")
endif ()

include(GNUInstallDirs)
include(CPack)

cmake_minimum_required(VERSION 4.1)

if(DEFINED KURT_ENABLED_VENDORS)
    message(FATAL_ERROR
        "KURT_ENABLED_VENDORS is no longer accepted by this entry, which builds "
        "host only. Use a clean build directory with the release or "
        "debug preset and omit KURT_ENABLED_VENDORS. Build CPU plugins "
        "separately from the vendor/ entry against an installed host package.")
endif()

project(kuai VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(KurtBuildOptions)
option(BUILD_SHARED_LIBS "Build the host as a shared library" OFF)
if(BUILD_SHARED_LIBS)
    set(KURT_LIB_TYPE SHARED)
else()
    set(KURT_LIB_TYPE STATIC)
endif()

option(KURT_ENABLE_BENCHMARK "enable benchmark" OFF)

option(KURT_INSTALL_CMAKE_PACKAGE "install the kuai CMake config package"
       ON)

if(KURT_INSTALL_CMAKE_PACKAGE)
    set(KURT_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/kuai"
        CACHE STRING "install location for the kuai CMake package")
endif()

include(KurtDependencies)

add_subdirectory(src)  # generate kurt target

if(KURT_ENABLE_BENCHMARK)
    add_subdirectory(tools/benchmarks)
endif()

if(KURT_INSTALL_CMAKE_PACKAGE)
    include(CMakePackageConfigHelpers)

    install(TARGETS kurt
        EXPORT kuaiTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT KuRuntime
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuRuntime
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuRuntime
        FILE_SET public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT KuRuntime)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/kuaiConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfig.cmake"
        INSTALL_DESTINATION "${KURT_INSTALL_CMAKEDIR}")

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY ExactVersion)

    install(EXPORT kuaiTargets
        FILE kuaiTargets.cmake
        NAMESPACE kuai::
        DESTINATION "${KURT_INSTALL_CMAKEDIR}"
        COMPONENT KuRuntime)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfigVersion.cmake"
        DESTINATION "${KURT_INSTALL_CMAKEDIR}"
        COMPONENT KuRuntime)

endif()

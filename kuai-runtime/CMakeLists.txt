cmake_minimum_required(VERSION 4.1)
project(kuai LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(KURT_LIB_TYPE SHARED)

set(KURT_WERROR_LIST
    return-type
    maybe-uninitialized
)

option(KURT_ENABLE_TRACY "enable tracy profiler" OFF)
option(KURT_ENABLE_BENCHMARK "enable benchmark" OFF)

option(KURT_INSTALL_CMAKE_PACKAGE "install the kuai CMake config package"
       ON)

if(KURT_ENABLE_TRACY)
set(TRACY_STATIC OFF)
include(FetchContent)
# fetch tracy using FetchContent
FetchContent_Declare(
tracy
GIT_REPOSITORY https://github.com/wolfpld/tracy.git
GIT_TAG v0.12.0
GIT_SHALLOW TRUE
GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

endif()

set(KURT_WARNING_EXCLUDE_LIST
    no-overloaded-virtual
    no-signed-compare
    no-misleading-indentation
    no-unused-value
    no-unused-variable
    no-unused-function
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    foreach(WARN_FLAG ${KURT_WERROR_LIST})
        list(APPEND KURT_WERROR_FLAG -Werror=${WARN_FLAG})
    endforeach()
    foreach(WARN_FLAG ${KURT_WARNING_EXCLUDE_LIST})
        list(APPEND KURT_WARNING_EXCLUDE_FLAG -W${WARN_FLAG})
    endforeach()
else()
    set(KURT_WERROR_FLAG "")
endif()

set(KURT_SUPPORTED_VENDORS "cpu;cuda")

# KURT_ENABLED_VENDORS must be a subset of KURT_SUPPORTED_VENDORS
set(KURT_ENABLED_VENDORS "cpu;cuda" CACHE STRING "The vendors enabled for kuai")

add_subdirectory(src)  # generate kurt target


# KURT_DEVICE_CPU
# KURT_DEVICE_CUDA

# ======================================================

add_subdirectory(vendor)  # generate kurt_{vendor} target

if(KURT_ENABLE_BENCHMARK)
    add_subdirectory(tools/benchmarks)
endif()

if(KURT_INSTALL_CMAKE_PACKAGE)
    include(CMakePackageConfigHelpers)

    set(KURT_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/kuai"
        CACHE STRING "install location for the kuai CMake package")

    install(TARGETS kurt
        EXPORT kuaiTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT KuRuntime
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuRuntime
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuRuntime
        FILE_SET public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT KuRuntime)

    foreach(vendor IN LISTS KURT_ENABLED_VENDORS)
        if(TARGET kurt_${vendor})
            install(TARGETS kurt_${vendor}
                RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT KuBackend
                LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuBackend
                ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT KuBackend)
        endif()
    endforeach()

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/kuaiConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfig.cmake"
        INSTALL_DESTINATION "${KURT_INSTALL_CMAKEDIR}")

    install(EXPORT kuaiTargets
        FILE kuaiTargets.cmake
        NAMESPACE kuai::
        DESTINATION "${KURT_INSTALL_CMAKEDIR}"
        COMPONENT KuRuntime)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kuaiConfig.cmake"
        DESTINATION "${KURT_INSTALL_CMAKEDIR}"
        COMPONENT KuRuntime)

endif()

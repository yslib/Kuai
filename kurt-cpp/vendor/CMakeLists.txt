cmake_minimum_required(VERSION 4.1)
project(kuai_vendors VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(KurtBuildOptions)

set(KURT_ENABLED_VENDORS cpu CACHE STRING "Vendors to build (semicolon-separated)")
set(KURT_SUPPORTED_VENDORS cpu cuda)
if("${KURT_ENABLED_VENDORS}" STREQUAL "")
    message(FATAL_ERROR "KURT_ENABLED_VENDORS must select at least one vendor: cpu, cuda")
endif()
foreach(vendor IN LISTS KURT_ENABLED_VENDORS)
    if(NOT vendor IN_LIST KURT_SUPPORTED_VENDORS)
        message(FATAL_ERROR "Vendor '${vendor}' is not supported by kurt. Supported names: cpu, cuda")
    endif()
endforeach()
list(REMOVE_DUPLICATES KURT_ENABLED_VENDORS)

find_package(kuai 0.1.0 EXACT CONFIG REQUIRED)
include(KurtVendorPackage)

foreach(vendor IN LISTS KURT_ENABLED_VENDORS)
    set(vendor_directory "${vendor}")
    if(vendor STREQUAL "cuda")
        set(vendor_directory nvidia)
    endif()
    add_library(kurt_${vendor}_interface INTERFACE)
    add_library(kuai::kurt_${vendor}_private ALIAS kurt_${vendor}_interface)
    target_include_directories(kurt_${vendor}_interface INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/${vendor_directory}/include")
    message(STATUS "Building kurt vendor: ${vendor}")
    add_subdirectory(${vendor_directory})

    # The host discovers only kuVendorModule by name. All other backend capabilities are
    # installed through the module's C callback tables and remain private to the vendor DSO.
    set_target_properties(kurt_${vendor}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES)
    if(APPLE)
        target_link_options(kurt_${vendor} PRIVATE
            "LINKER:-exported_symbol,_kuVendorModule")
    elseif(UNIX)
        target_link_options(kurt_${vendor} PRIVATE
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/KuVendorExports.map")
    endif()
    add_library(kuai::kurt_${vendor} ALIAS kurt_${vendor})
    kurt_install_vendor_package(${vendor})
endforeach()

message(STATUS "Configuring kuai vendor interfaces...")
foreach(vendor IN LISTS KURT_ENABLED_VENDORS)
    if(NOT vendor IN_LIST KURT_SUPPORTED_VENDORS)
        message(FATAL_ERROR "Vendor '${vendor}' is not supported by kurt.")
    endif()

    string(TOLOWER ${vendor} vendor_lower)
    string(TOUPPER ${vendor} vendor_upper)
    string(TOLOWER ${CMAKE_BUILD_TYPE} build_type_lower)

    if(vendor_lower STREQUAL "cpu")
        set(vendor_source_dir cpu)
    elseif(vendor_lower STREQUAL "cuda")
        set(vendor_source_dir nvidia)
    else()
        message(FATAL_ERROR "Vendor '${vendor}' has no vendor source directory mapping.")
    endif()

    add_library(kurt_${vendor}_interface INTERFACE)
    add_library(kuai::kurt_${vendor}_private ALIAS kurt_${vendor}_interface)
    target_include_directories(kurt_${vendor}_interface INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/${vendor_source_dir}/include
        ${PROJECT_SOURCE_DIR}/src/kuai_c/private)
    target_link_libraries(kurt_${vendor}_interface INTERFACE
        kuai::core::private
        kuai::profiler)
    message(STATUS "Building kurt for vendor: ${vendor_upper}")
    add_subdirectory(${vendor_source_dir})

    # The host discovers only kuVendorModule by name. All other backend capabilities are
    # installed through the module's C callback tables and remain private to the vendor DSO.
    set_target_properties(kurt_${vendor}
        PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES)
    if(vendor_lower STREQUAL "cuda")
        set_target_properties(kurt_${vendor}
            PROPERTIES CUDA_VISIBILITY_PRESET hidden)
    endif()
    if(APPLE)
        target_link_options(kurt_${vendor} PRIVATE
            "LINKER:-exported_symbol,_kuVendorModule")
    elseif(UNIX)
        target_link_options(kurt_${vendor} PRIVATE
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/KuVendorExports.map")
    endif()
    add_library(kuai::kurt_${vendor} ALIAS kurt_${vendor})
endforeach()

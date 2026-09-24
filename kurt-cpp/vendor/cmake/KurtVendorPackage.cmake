include_guard(GLOBAL)
include(CMakePackageConfigHelpers)

function(kurt_install_vendor_package backend)
    set(vendor_package "kuai_vendor_${backend}")
    set(package_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${vendor_package}")
    set(package_build_dir "${CMAKE_CURRENT_BINARY_DIR}/${vendor_package}")

    # MODULE DLLs are LIBRARY artifacts in CMake, including on Windows.
    if(WIN32)
        set(plugin_install_dir "${CMAKE_INSTALL_BINDIR}")
    else()
        set(plugin_install_dir "${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(APPLE)
        set(plugin_install_rpath "@loader_path")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(plugin_install_rpath "$ORIGIN")
    else()
        set(plugin_install_rpath "")
    endif()
    set_target_properties(kurt_${backend} PROPERTIES
        INSTALL_RPATH "${plugin_install_rpath}"
        INSTALL_RPATH_USE_LINK_PATH FALSE)
    install(TARGETS kurt_${backend}
        EXPORT ${vendor_package}Targets
        LIBRARY DESTINATION "${plugin_install_dir}" COMPONENT KuRuntime)

    configure_package_config_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/kuaiVendorConfig.cmake.in"
        "${package_build_dir}/${vendor_package}Config.cmake"
        INSTALL_DESTINATION "${package_install_dir}")
    write_basic_package_version_file(
        "${package_build_dir}/${vendor_package}ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY ExactVersion)
    install(EXPORT ${vendor_package}Targets
        FILE ${vendor_package}Targets.cmake
        NAMESPACE kuai::
        DESTINATION "${package_install_dir}"
        COMPONENT KuRuntime)
    install(FILES
        "${package_build_dir}/${vendor_package}Config.cmake"
        "${package_build_dir}/${vendor_package}ConfigVersion.cmake"
        DESTINATION "${package_install_dir}"
        COMPONENT KuRuntime)
endfunction()

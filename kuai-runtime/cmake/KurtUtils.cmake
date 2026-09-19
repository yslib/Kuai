# KurtUtils.cmake
# Automatically detect and add a module to the build system.
#
# Module directory structure:
# <any_name>/include/<package_name>/<module_name>/*.h for public headers.
# <any_name>/private/<module_name>/*.h for private headers.
# <any_name>/src/**/*.cpp for source files.
#
# By default, kurt_add_module treats the current source directory as the module
# directory. Pass MODULE_PATH to point it at another directory with the same
# module structure, or SOURCE_DIR to select a source subtree within it. Modules
# default to OBJECT; STATIC, SHARED, OBJECT, and INTERFACE select an explicit
# target kind. TARGET_NAME changes the CMake target name without changing the
# logical module name used to validate the directory layout.

function(_kurt_resolve_paths out_var)
    set(_paths)
    foreach(_path ${ARGN})
        if(IS_ABSOLUTE "${_path}")
            set(_resolved_path "${_path}")
        else()
            set(_resolved_path "${CMAKE_CURRENT_SOURCE_DIR}/${_path}")
        endif()
        get_filename_component(_resolved_path "${_resolved_path}" ABSOLUTE)
        list(APPEND _paths "${_resolved_path}")
    endforeach()
    set(${out_var} ${_paths} PARENT_SCOPE)
endfunction()

function(_kurt_glob_headers out_var)
    set(_headers)
    foreach(_include_dir ${ARGN})
        file(GLOB_RECURSE _include_headers CONFIGURE_DEPENDS
            "${_include_dir}/*.h"
            "${_include_dir}/*.hpp")
        list(APPEND _headers ${_include_headers})
    endforeach()
    set(${out_var} ${_headers} PARENT_SCOPE)
endfunction()

function(kurt_add_library target_name)
    set(options STATIC SHARED OBJECT INTERFACE)
    set(oneValueArgs
        CXX_STANDARD
        CXX_STANDARD_REQUIRED
        POSITION_INDEPENDENT_CODE
        EXPORT_NAME)
    set(multiValueArgs
        SOURCES
        PUBLIC_INCLUDE_DIRS
        PRIVATE_INCLUDE_DIRS
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
        PUBLIC_LINK_LIBRARIES
        PRIVATE_LINK_LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_INTERFACE AND NOT ARG_STATIC AND NOT ARG_SHARED AND NOT ARG_OBJECT)
        set(ARG_STATIC TRUE)
    endif()
    if(NOT ARG_INTERFACE AND ((ARG_STATIC AND ARG_SHARED) OR (ARG_STATIC AND ARG_OBJECT) OR (ARG_SHARED AND ARG_OBJECT)))
        message(FATAL_ERROR "Library ${target_name} must have exactly one library kind")
    endif()
    if(NOT ARG_CXX_STANDARD)
        set(ARG_CXX_STANDARD 23)
    endif()
    if(NOT DEFINED ARG_CXX_STANDARD_REQUIRED)
        set(ARG_CXX_STANDARD_REQUIRED ON)
    endif()
    if(NOT DEFINED ARG_POSITION_INDEPENDENT_CODE)
        set(ARG_POSITION_INDEPENDENT_CODE ON)
    endif()
    if(NOT ARG_EXPORT_NAME)
        set(ARG_EXPORT_NAME ${target_name})
    endif()

    set(KURT_NAMESPACE kuai)

    if(ARG_INTERFACE)
        add_library(${target_name} INTERFACE)
    elseif(ARG_STATIC)
        add_library(${target_name} STATIC)
    elseif(ARG_SHARED)
        add_library(${target_name} SHARED)
    elseif(ARG_OBJECT)
        add_library(${target_name} OBJECT)
    endif()

    if(ARG_INTERFACE)
        set(_TARGET_SOURCE_SCOPE INTERFACE)
        set(_TARGET_PUBLIC_SCOPE INTERFACE)
        set(_TARGET_PRIVATE_SCOPE INTERFACE)
        set(_TARGET_PUBLIC_LINK_SCOPE INTERFACE)
        set(_TARGET_PRIVATE_LINK_SCOPE INTERFACE)
        set(_TARGET_COMPILE_SCOPE INTERFACE)
    else()
        set(_TARGET_SOURCE_SCOPE PRIVATE)
        set(_TARGET_PUBLIC_SCOPE PUBLIC)
        set(_TARGET_PRIVATE_SCOPE PRIVATE)
        set(_TARGET_PUBLIC_LINK_SCOPE PUBLIC)
        set(_TARGET_PRIVATE_LINK_SCOPE PRIVATE)
        set(_TARGET_COMPILE_SCOPE PRIVATE)
    endif()

    add_library(${KURT_NAMESPACE}::${target_name} ALIAS ${target_name})

    add_library(${target_name}_private INTERFACE)
    add_library(${KURT_NAMESPACE}::${target_name}::private ALIAS ${target_name}_private)
    target_link_libraries(${target_name}_private INTERFACE ${target_name})

    _kurt_resolve_paths(_PUBLIC_INCLUDE_DIRS ${ARG_PUBLIC_INCLUDE_DIRS})
    _kurt_resolve_paths(_PRIVATE_INCLUDE_DIRS ${ARG_PRIVATE_INCLUDE_DIRS})
    _kurt_glob_headers(_PUBLIC_HEADERS ${_PUBLIC_INCLUDE_DIRS})
    _kurt_glob_headers(_PRIVATE_HEADERS ${_PRIVATE_INCLUDE_DIRS})

    if(ARG_SOURCES)
        target_sources(${target_name}
            ${_TARGET_SOURCE_SCOPE} ${ARG_SOURCES})
    endif()

    if(_PUBLIC_INCLUDE_DIRS)
        target_sources(${target_name}
            ${_TARGET_PUBLIC_SCOPE}
                FILE_SET public_headers
                TYPE HEADERS
                BASE_DIRS ${_PUBLIC_INCLUDE_DIRS}
                FILES ${_PUBLIC_HEADERS})
        target_sources(${target_name}_private
            INTERFACE
                FILE_SET public_headers
                TYPE HEADERS
                BASE_DIRS ${_PUBLIC_INCLUDE_DIRS}
                FILES ${_PUBLIC_HEADERS})
    endif()

    if(_PRIVATE_INCLUDE_DIRS)
        target_sources(${target_name}
            ${_TARGET_PRIVATE_SCOPE}
                FILE_SET private_headers
                TYPE HEADERS
                BASE_DIRS ${_PRIVATE_INCLUDE_DIRS}
                FILES ${_PRIVATE_HEADERS})
        target_sources(${target_name}_private
            INTERFACE
                FILE_SET private_headers
                TYPE HEADERS
                BASE_DIRS ${_PRIVATE_INCLUDE_DIRS}
                FILES ${_PRIVATE_HEADERS})
    endif()

    if(ARG_INTERFACE)
        set_target_properties(${target_name} PROPERTIES
            EXPORT_NAME ${ARG_EXPORT_NAME})
    else()
        set_target_properties(${target_name} PROPERTIES
            CXX_STANDARD ${ARG_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ${ARG_CXX_STANDARD_REQUIRED}
            POSITION_INDEPENDENT_CODE ${ARG_POSITION_INDEPENDENT_CODE}
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            EXPORT_NAME ${ARG_EXPORT_NAME})
    endif()

    if(ARG_COMPILE_DEFINITIONS)
        target_compile_definitions(${target_name} ${_TARGET_COMPILE_SCOPE} ${ARG_COMPILE_DEFINITIONS})
    endif()

    if(ARG_COMPILE_OPTIONS)
        target_compile_options(${target_name} ${_TARGET_COMPILE_SCOPE} ${ARG_COMPILE_OPTIONS})
    endif()

    if(ARG_PUBLIC_LINK_LIBRARIES)
        target_link_libraries(${target_name} ${_TARGET_PUBLIC_LINK_SCOPE} ${ARG_PUBLIC_LINK_LIBRARIES})
    endif()

    if(ARG_PRIVATE_LINK_LIBRARIES)
        target_link_libraries(${target_name} ${_TARGET_PRIVATE_LINK_SCOPE} ${ARG_PRIVATE_LINK_LIBRARIES})
    endif()
endfunction()

function(kurt_add_module module_name)
    set(options STATIC SHARED OBJECT INTERFACE)
    set(oneValueArgs
        MODULE_PATH
        SOURCE_DIR
        TARGET_NAME
        CXX_STANDARD
        CXX_STANDARD_REQUIRED
        POSITION_INDEPENDENT_CODE
        EXPORT_NAME)
    set(multiValueArgs
        SOURCES
        PUBLIC_INCLUDE_DIRS
        PRIVATE_INCLUDE_DIRS
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
        PRIVATE_LINK_LIBRARIES
        LINK_LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(_MODULE_KIND_COUNT 0)
    foreach(_MODULE_KIND STATIC SHARED OBJECT INTERFACE)
        if(ARG_${_MODULE_KIND})
            math(EXPR _MODULE_KIND_COUNT "${_MODULE_KIND_COUNT} + 1")
        endif()
    endforeach()
    if(_MODULE_KIND_COUNT GREATER 1)
        message(FATAL_ERROR "Module ${module_name} must have exactly one library kind")
    endif()
    if(_MODULE_KIND_COUNT EQUAL 0)
        set(ARG_OBJECT TRUE)
    endif()

    if(DEFINED ARG_TARGET_NAME AND NOT "${ARG_TARGET_NAME}" STREQUAL "")
        set(_MODULE_TARGET_NAME "${ARG_TARGET_NAME}")
    else()
        set(_MODULE_TARGET_NAME "${module_name}")
    endif()

    if(DEFINED ARG_MODULE_PATH AND NOT "${ARG_MODULE_PATH}" STREQUAL "")
        if(IS_ABSOLUTE "${ARG_MODULE_PATH}")
            set(_MODULE_PATH "${ARG_MODULE_PATH}")
        else()
            set(_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_MODULE_PATH}")
        endif()
    else()
        set(_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    get_filename_component(_MODULE_PATH "${_MODULE_PATH}" ABSOLUTE)

    set(KURT_NAMESPACE kuai)

    set(_MODULE_INCLUDE_DIR "${_MODULE_PATH}/include")
    if(EXISTS "${_MODULE_INCLUDE_DIR}")
        if(NOT EXISTS "${_MODULE_INCLUDE_DIR}/${KURT_NAMESPACE}/${module_name}")
            message(FATAL_ERROR
                "Module ${module_name} include directory must have the structure "
                "include/${KURT_NAMESPACE}/${module_name}")
        endif()
        list(APPEND ARG_PUBLIC_INCLUDE_DIRS "${_MODULE_INCLUDE_DIR}")
    endif()

    set(_MODULE_PRIVATE_DIR "${_MODULE_PATH}/private")
    if(EXISTS "${_MODULE_PRIVATE_DIR}")
        if(NOT EXISTS "${_MODULE_PRIVATE_DIR}/${module_name}")
            message(FATAL_ERROR
                "Module ${module_name} private directory must have the structure "
                "private/${module_name}")
        endif()
        list(APPEND ARG_PRIVATE_INCLUDE_DIRS "${_MODULE_PRIVATE_DIR}")
    endif()

    set(_MODULE_SOURCES)
    if(DEFINED ARG_SOURCE_DIR AND NOT "${ARG_SOURCE_DIR}" STREQUAL "")
        if(IS_ABSOLUTE "${ARG_SOURCE_DIR}")
            set(_MODULE_SOURCE_DIR "${ARG_SOURCE_DIR}")
        else()
            set(_MODULE_SOURCE_DIR "${_MODULE_PATH}/${ARG_SOURCE_DIR}")
        endif()
    else()
        set(_MODULE_SOURCE_DIR "${_MODULE_PATH}/src")
    endif()
    if(EXISTS "${_MODULE_SOURCE_DIR}")
        file(GLOB_RECURSE _MODULE_SOURCES CONFIGURE_DEPENDS
            "${_MODULE_SOURCE_DIR}/*.cc"
            "${_MODULE_SOURCE_DIR}/*.cpp"
            "${_MODULE_SOURCE_DIR}/*.cxx")
    elseif(NOT ARG_INTERFACE AND NOT ARG_SOURCES)
        message(FATAL_ERROR "Module ${module_name} must have a src directory or explicit SOURCES")
    endif()
    list(APPEND _MODULE_SOURCES ${ARG_SOURCES})

    if(ARG_STATIC)
        set(_KURT_LIBRARY_ARGS STATIC)
    elseif(ARG_SHARED)
        set(_KURT_LIBRARY_ARGS SHARED)
    elseif(ARG_OBJECT)
        set(_KURT_LIBRARY_ARGS OBJECT)
    elseif(ARG_INTERFACE)
        set(_KURT_LIBRARY_ARGS INTERFACE)
    endif()
    if(DEFINED ARG_CXX_STANDARD)
        list(APPEND _KURT_LIBRARY_ARGS CXX_STANDARD ${ARG_CXX_STANDARD})
    endif()
    if(DEFINED ARG_CXX_STANDARD_REQUIRED)
        list(APPEND _KURT_LIBRARY_ARGS CXX_STANDARD_REQUIRED ${ARG_CXX_STANDARD_REQUIRED})
    endif()
    if(DEFINED ARG_POSITION_INDEPENDENT_CODE)
        list(APPEND _KURT_LIBRARY_ARGS POSITION_INDEPENDENT_CODE ${ARG_POSITION_INDEPENDENT_CODE})
    endif()
    if(DEFINED ARG_EXPORT_NAME)
        list(APPEND _KURT_LIBRARY_ARGS EXPORT_NAME ${ARG_EXPORT_NAME})
    endif()
    if(ARG_LINK_LIBRARIES)
        list(APPEND ARG_PRIVATE_LINK_LIBRARIES ${ARG_LINK_LIBRARIES})
    endif()

    kurt_add_library(${_MODULE_TARGET_NAME}
        ${_KURT_LIBRARY_ARGS}
        SOURCES
            ${_MODULE_SOURCES}
        PUBLIC_INCLUDE_DIRS
            ${ARG_PUBLIC_INCLUDE_DIRS}
        PRIVATE_INCLUDE_DIRS
            ${ARG_PRIVATE_INCLUDE_DIRS}
        COMPILE_DEFINITIONS
            ${ARG_COMPILE_DEFINITIONS}
        COMPILE_OPTIONS
            ${ARG_COMPILE_OPTIONS}
        PRIVATE_LINK_LIBRARIES
            ${ARG_PRIVATE_LINK_LIBRARIES})
endfunction()

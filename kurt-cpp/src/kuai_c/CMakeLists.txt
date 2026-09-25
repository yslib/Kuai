include(KurtUtils)

kurt_add_module(kuai_c
    TARGET_NAME kurt
    ${KURT_LIB_TYPE}
    PRIVATE_INCLUDE_DIRS
        ${PROJECT_SOURCE_DIR}/src/core/private
        ${PROJECT_SOURCE_DIR}/src/runtime/private
    COMPILE_OPTIONS
        ${KURT_WERROR_FLAG}
    PRIVATE_LINK_LIBRARIES
        $<BUILD_INTERFACE:kuai::core>
        $<BUILD_INTERFACE:kuai::runtime>
        $<BUILD_INTERFACE:kuai::profiler>
        Threads::Threads
        ${CMAKE_DL_LIBS}
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)

# The aggregate target exposes every installed C++ module from the build tree.
# The install tree already has the same unified include prefix.
target_include_directories(kurt PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/runtime/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/profiler/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(kurt INTERFACE $<INSTALL_INTERFACE:cxx_std_${CMAKE_CXX_STANDARD}>)

set_target_properties(kurt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

cmake_minimum_required(VERSION 4.1)
project(kupy LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
find_package(kuai CONFIG REQUIRED)
find_package(Python REQUIRED COMPONENTS Development.Module Interpreter)
find_package(pybind11 CONFIG REQUIRED)
file(GLOB_RECURSE KUPY_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*.cpp")

pybind11_add_module(kupy_native MODULE ${KUPY_SRC})
target_link_libraries(kupy_native PRIVATE kuai::kurt)

set_target_properties(
    kupy_native PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "_native"
    PREFIX "")

install(TARGETS kupy_native
    LIBRARY DESTINATION kupy
    RUNTIME DESTINATION kupy)

install(
    DIRECTORY "$<TARGET_FILE_DIR:kuai::kurt>/"
    DESTINATION kupy
    FILES_MATCHING
    PATTERN "libkurt*.so*")

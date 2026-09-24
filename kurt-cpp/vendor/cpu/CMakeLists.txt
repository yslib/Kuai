file(GLOB_RECURSE KURT_CPU_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(kurt_cpu MODULE ${KURT_CPU_FILES})
target_link_libraries(kurt_cpu
    PRIVATE kuai::kurt kuai::kurt_cpu_private)
target_include_directories(kurt_cpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private)
target_compile_options(kurt_cpu PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-fpermissive>)
# some exclusive compile options for our targets
set_target_properties(
    kurt_cpu
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_STANDARD 26
        CXX_STANDARD_REQUIRED ON
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" # Output the
        OUTPUT_NAME kurt_cpu)

# MODULE defaults to .so on macOS; the existing host loader expects .dylib.
if(APPLE)
    set_target_properties(kurt_cpu PROPERTIES SUFFIX ".dylib")
endif()

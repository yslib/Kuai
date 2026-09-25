include(KurtUtils)
kurt_add_module(core
    OBJECT
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/kuai_c/include>)

target_link_libraries(core PUBLIC kuai::profiler)

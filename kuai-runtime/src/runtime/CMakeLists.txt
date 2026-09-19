include(KurtUtils)
kurt_add_module(runtime
    OBJECT
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
    PRIVATE_LINK_LIBRARIES
        kuai::core::private)

target_link_libraries(runtime PUBLIC
    kuai::core
    kuai::profiler)

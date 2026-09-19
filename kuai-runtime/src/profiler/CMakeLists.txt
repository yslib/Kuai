include(KurtUtils)
kurt_add_module(profiler INTERFACE)

if(KURT_ENABLE_TRACY)
    target_compile_definitions(profiler INTERFACE KURT_ENABLE_TRACY)
    target_link_libraries(profiler INTERFACE TracyClient)
endif()

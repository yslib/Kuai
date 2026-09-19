include(KurtUtils)

function(kurt_install_public_headers)
    install(TARGETS ${ARGV}
        FILE_SET public_headers
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT KuRuntime)
endfunction()

add_subdirectory(profiler)
add_subdirectory(core)
add_subdirectory(runtime)
add_subdirectory(kuai_c)

if(KURT_INSTALL_CMAKE_PACKAGE)
    kurt_install_public_headers(core runtime profiler)
endif()

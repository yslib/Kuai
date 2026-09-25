set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake maps MSVC's C++23 mode to /std:c++latest; cxx_std_26 is not available.
if(MSVC)
    set(CMAKE_CXX_STANDARD 23)
else()
    set(CMAKE_CXX_STANDARD 26)
endif()

set(KURT_WERROR_LIST
    return-type
    maybe-uninitialized
)

set(KURT_WARNING_EXCLUDE_LIST
    no-overloaded-virtual
    no-signed-compare
    no-misleading-indentation
    no-unused-value
    no-unused-variable
    no-unused-function
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    foreach(WARN_FLAG ${KURT_WERROR_LIST})
        list(APPEND KURT_WERROR_FLAG -Werror=${WARN_FLAG})
    endforeach()
    foreach(WARN_FLAG ${KURT_WARNING_EXCLUDE_LIST})
        list(APPEND KURT_WARNING_EXCLUDE_FLAG -W${WARN_FLAG})
    endforeach()
else()
    set(KURT_WERROR_FLAG "")
endif()

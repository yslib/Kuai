set(CMAKE_CXX_COMPILER cl)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_CXX_FLAGS_INIT "/w /permissive- /Zc:preprocessor /Zc:__cplusplus /utf-8 /bigobj /DNOMINMAX /DWIN32_LEAN_AND_MEAN")

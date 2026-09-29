include("${CMAKE_CURRENT_LIST_DIR}/windows-msvc.cmake")

set(CMAKE_CUDA_COMPILER nvcc)
set(CMAKE_CUDA_HOST_COMPILER cl)
set(CMAKE_CUDA_FLAGS_INIT "-Xcompiler=/bigobj")

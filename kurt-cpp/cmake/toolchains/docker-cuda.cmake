include("${CMAKE_CURRENT_LIST_DIR}/docker-base.cmake")

set(CMAKE_CUDA_COMPILER "${CMAKE_CXX_COMPILER}")
set(CUDAToolkit_ROOT /usr/local/cuda)

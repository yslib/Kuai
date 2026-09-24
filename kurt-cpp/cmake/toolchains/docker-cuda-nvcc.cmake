include("${CMAKE_CURRENT_LIST_DIR}/docker-cuda.cmake")

set(CMAKE_CUDA_COMPILER "${CUDAToolkit_ROOT}/bin/nvcc")
set(CMAKE_CUDA_HOST_COMPILER g++)

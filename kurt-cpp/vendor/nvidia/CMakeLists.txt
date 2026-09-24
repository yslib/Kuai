enable_language(CUDA)

find_package(CUDAToolkit REQUIRED)

file(GLOB_RECURSE KURT_CUDA_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cu)

add_library(kurt_cuda MODULE ${KURT_CUDA_FILES})
target_link_libraries(kurt_cuda
    PRIVATE
        kuai::kurt
        kuai::kurt_cuda_private
        CUDA::curand
        CUDA::cublas
        CUDA::cusolver)

target_include_directories(kurt_cuda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private)

target_compile_definitions(kurt_cuda
    PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:LIBCUDACXX_ENABLE_EXPERIMENTAL_MEMORY_RESOURCE>
)

# kuai device code intentionally uses constexpr facilities from the C++
# standard library. NVCC requires this opt-in; Clang CUDA accepts them directly.
target_compile_options(kurt_cuda PRIVATE
    $<$<COMPILE_LANG_AND_ID:CUDA,NVIDIA>:--expt-relaxed-constexpr>)

message(STATUS "COMPILING FOR CUDA ARCHITECTURES: ${CMAKE_CUDA_ARCHITECTURES}")

set_target_properties(
    kurt_cuda
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
        CUDA_RUNTIME_LIBRARY Shared
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        OUTPUT_NAME kurt_cuda)

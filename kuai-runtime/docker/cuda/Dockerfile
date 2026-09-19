ARG BASE_IMAGE=kuai-toolchain:base

FROM ${BASE_IMAGE} AS cuda-sdk

COPY docker/cuda/components.lock docker/cuda/setup-sdk.sh /tmp/cuda/
RUN --mount=type=cache,target=/var/cache/kuai-cuda,sharing=locked \
    /tmp/cuda/setup-sdk.sh \
        /usr/local/cuda-12.8 \
        /var/cache/kuai-cuda \
        /tmp/cuda/components.lock

FROM ${BASE_IMAGE}

COPY --from=cuda-sdk /usr/local/cuda-12.8 /usr/local/cuda-12.8

RUN ln -s cuda-12.8 /usr/local/cuda

ENV CUDA_HOME=/usr/local/cuda \
    CUDACXX=/opt/llvm/bin/clang++ \
    PATH=/usr/local/cuda/bin:${PATH} \
    LD_LIBRARY_PATH=/usr/local/cuda/lib64

ARG MANYLINUX_IMAGE=quay.io/pypa/manylinux_2_28_x86_64@sha256:35795c40ab9eed587a0f51a752c7defeabcc8a2ccd434d3497be201cdd34cb63

FROM ${MANYLINUX_IMAGE} AS llvm-builder

ARG LLVM_VERSION=22.1.8
ARG LLVM_SHA256=922f1817a0df7b1489272d18134ee0087a8b068828f87ac63b9861b1a9965888
ARG LLVM_BUILD_JOBS=16

ENV PATH=/opt/python/cp314-cp314/bin:${PATH}

RUN python -m pip install --no-cache-dir ninja==1.13.0

WORKDIR /tmp/llvm
RUN curl --fail --location --retry 5 \
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz" \
        --output llvm-project.tar.xz \
    && echo "${LLVM_SHA256}  llvm-project.tar.xz" | sha256sum --check \
    && tar --extract --file llvm-project.tar.xz \
    && rm llvm-project.tar.xz

RUN cmake \
        -S "llvm-project-${LLVM_VERSION}.src/llvm" \
        -B build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/llvm \
        -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
        -DLLVM_TARGETS_TO_BUILD="X86;NVPTX" \
        -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
        -DLLVM_ENABLE_ASSERTIONS=OFF \
        -DLLVM_ENABLE_TERMINFO=OFF \
        -DLLVM_ENABLE_ZLIB=OFF \
        -DLLVM_ENABLE_ZSTD=OFF \
        -DLLVM_ENABLE_LIBXML2=OFF \
        -DLLVM_INCLUDE_BENCHMARKS=OFF \
        -DLLVM_INCLUDE_EXAMPLES=OFF \
        -DLLVM_INCLUDE_TESTS=OFF \
    && cmake --build build --target install --parallel "${LLVM_BUILD_JOBS}" \
    && rm -rf \
        /tmp/llvm/build \
        "/tmp/llvm/llvm-project-${LLVM_VERSION}.src" \
    && printf '%s\n' \
        '--gcc-install-dir=/opt/rh/gcc-toolset-14/root/usr/lib/gcc/x86_64-redhat-linux/14' \
        > /opt/llvm/bin/x86_64-unknown-linux-gnu.cfg

FROM ${MANYLINUX_IMAGE}

COPY --from=llvm-builder /opt/llvm /opt/llvm

RUN /opt/python/cp313-cp313/bin/python -m venv /opt/venv313 \
    && /opt/python/cp314-cp314/bin/python -m venv /opt/venv314 \
    && /opt/venv313/bin/python -m pip install --no-cache-dir \
        build==1.5.0 \
        ninja==1.13.0 \
        numpy==2.5.2 \
        pybind11==3.1.0 \
        scikit-build-core==1.0.3 \
    && /opt/venv314/bin/python -m pip install --no-cache-dir \
        build==1.5.0 \
        ninja==1.13.0 \
        numpy==2.5.2 \
        pybind11==3.1.0 \
        scikit-build-core==1.0.3

ENV PATH=/opt/llvm/bin:/opt/venv314/bin:${PATH} \
    CC=/opt/llvm/bin/clang \
    CXX=/opt/llvm/bin/clang++ \
    AR=/opt/llvm/bin/llvm-ar \
    NM=/opt/llvm/bin/llvm-nm \
    RANLIB=/opt/llvm/bin/llvm-ranlib \
    STRIP=/opt/llvm/bin/llvm-strip

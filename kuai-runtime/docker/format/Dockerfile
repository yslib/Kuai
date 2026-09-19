ARG UBUNTU_IMAGE=ubuntu:25.10@sha256:7cc5e35f6567ee8c66d2abb4aab0fd866669e6207c237c3a8f0947a5c7f17092

FROM ${UBUNTU_IMAGE}

ARG LLVM_MAJOR_VERSION=21
ARG CLANG_FORMAT_VERSION=21.1.8

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl gnupg \
    && curl --fail --location --retry 5 https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor --output /usr/share/keyrings/llvm-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] https://apt.llvm.org/questing/ llvm-toolchain-questing-${LLVM_MAJOR_VERSION} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends "clang-format-${LLVM_MAJOR_VERSION}" \
    && ln -s "/usr/bin/clang-format-${LLVM_MAJOR_VERSION}" /usr/bin/clang-format \
    && clang-format --version | grep --fixed-strings "clang-format version ${CLANG_FORMAT_VERSION}" \
    && apt-get purge -y --auto-remove curl gnupg \
    && rm -rf /var/lib/apt/lists/*

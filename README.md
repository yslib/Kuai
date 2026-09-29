<p align="center">
  <img src="assets/kuai.svg" alt="Kuai">
</p>

---

kuai, pronounced like kwai, it means fast(快) or block(块) in Chinese.

This is my personal project that has no goal, no roadmap, just for fun.

## Supported Platforms

| Platform | Architecture | Host compiler | CPU vendor | CUDA vendor | CI |
| --- | --- | --- | :---: | --- | --- |
| Linux | x86_64 | Clang 22 | ✓ | Clang 22 + CUDA Toolkit 12.8 | [![Linux x64](https://github.com/yslib/kuai/actions/workflows/linux.yml/badge.svg?branch=main&event=push)](https://github.com/yslib/kuai/actions/workflows/linux.yml?query=branch%3Amain+event%3Apush) |
| macOS | arm64 | Clang 22 (Homebrew LLVM) | ✓ | — | [![macOS arm64](https://github.com/yslib/kuai/actions/workflows/macos.yml/badge.svg?branch=main&event=push)](https://github.com/yslib/kuai/actions/workflows/macos.yml?query=branch%3Amain+event%3Apush) |
| Windows | x86_64 | MSVC v143 (VS 2022) | ✓ | NVCC 13.2 + MSVC v143 | [![Windows x64](https://github.com/yslib/kuai/actions/workflows/windows.yml/badge.svg?branch=main&event=push)](https://github.com/yslib/kuai/actions/workflows/windows.yml?query=branch%3Amain+event%3Apush) |

CPU vendors use the host compiler. CPU and CUDA can be enabled separately or together.

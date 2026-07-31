# CMake build

This repository now includes a CMake build that mirrors the existing Visual Studio targets for:

- x65
- dump_x65

## Build

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

On macOS and Linux, the build uses the standard C++17 toolchain and links the math library where needed.

## Notes

- The CMake project is intentionally simple and uses the existing source files directly.
- The output binaries are placed under the build tree by default.

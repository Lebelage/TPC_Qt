# TPC

C++23 library for OPC UA communication with the TPC subsystem and magnetic-field analytics.

## Project layout

- `include/tpc` — installed public headers;
- `src` — compiled implementation;
- `tests` — tests that do not require a live OPC UA server;
- `cmake` — installed CMake package configuration.

The primary public include is:

```cpp
#include <tpc/tpc.hpp>
```

The primary CMake target is `TPC::TPC`. Component targets `TPC::System`,
`TPC::Analytics`, `TPC::Core`, and `TPC::Utilities` are also available.

## Dependencies

The repository includes a `vcpkg.json` manifest for Eigen, open62541pp, and
stdexec. Configure CMake with your vcpkg toolchain, or make those packages
available through another CMake package provider.

## Build and test

```sh
cmake --preset debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset debug
ctest --preset debug
```

For an optimized build:

```sh
cmake --preset release -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset release
```

## Install and consume

```sh
cmake --install build/release --prefix /path/to/tpc-install
```

A downstream project can then use the exported package:

```cmake
find_package(TPC CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE TPC::TPC)
```

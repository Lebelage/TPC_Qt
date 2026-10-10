# Vendored OPC UA dependencies

These source files are committed with the application and compiled directly by
`third_party/xmake.lua`. Normal builds need neither an OPC UA package download
nor CMake/Python code generation.

| Library | Upstream tag | Included files | License |
| --- | --- | --- | --- |
| [open62541](https://github.com/open62541/open62541/tree/v1.4.20) | v1.4.20 | multi-architecture amalgamation, LICENSE, AUTHORS | MPL-2.0 |
| [open62541pp](https://github.com/open62541pp/open62541pp/tree/v0.21.3) | v0.21.3 | include/, src/, generated version/compatibility headers, LICENSE | MPL-2.0 |

Upstream archives, verified before importing:

- open62541: `https://github.com/open62541/open62541/archive/refs/tags/v1.4.20.tar.gz`
  SHA256: `1464937bfbb9310f1465477e62df676d2dd2b0c25315a60f333d6350c600a2cb`
- open62541pp: `https://github.com/open62541pp/open62541pp/archive/refs/tags/v0.21.3.tar.gz`
  SHA256: `f841fd85f61976d624099ac82e7c12c07a1d0316b96d81be18ecf53433a1e041`

## Updating open62541

Use a clean upstream source archive in a temporary directory. Generate its
single-file distribution with upstream tooling, then replace the two vendored
source files and attribution files. This is a maintainer operation, not part
of the application build:

```sh
cmake -S /path/to/open62541 -B /tmp/open62541-amalgamation -G Ninja \
  -DOPEN62541_VERSION=v1.4.20 -DCMAKE_BUILD_TYPE=Release \
  -DUA_ENABLE_AMALGAMATION=ON -DUA_AMALGAMATION_MULTIARCH=ON \
  -DUA_ENABLE_ENCRYPTION=OFF -DUA_ENABLE_METHODCALLS=ON \
  -DUA_ENABLE_SUBSCRIPTIONS=ON -DUA_ENABLE_SUBSCRIPTIONS_EVENTS=ON \
  -DUA_ENABLE_DEBUG_SANITIZER=OFF -DUA_BUILD_EXAMPLES=OFF \
  -DUA_BUILD_UNIT_TESTS=OFF -DUA_FORCE_WERROR=OFF
cmake --build /tmp/open62541-amalgamation \
  --target open62541-amalgamation-source open62541-amalgamation-header
```

The profile uses the upstream defaults for remaining features (including
namespace zero, node management, type descriptions and multithreading=100).
Multi-architecture output selects POSIX or Win32 at compile time. Feature
changes require regenerating both files; consumers must use the same header.
The sources are not patched. The xmake target supplies Darwin feature flags
and the Apple ARM64 IEEE754 definitions absent from upstream 1.4 detection.

## Updating open62541pp

Copy upstream `include/`, `src/` and `LICENSE`. Generate `config.hpp` from
`config.hpp.in` with the upstream version, and the compatibility headers
`nodeids.hpp`, `types_composed.hpp`, `typewrapper.hpp` from `deprecated.hpp.in`
using `ua/nodeids.hpp`, `ua/types.hpp`, `wrapper.hpp` respectively. Remove the
unused `.in` templates from the imported copy. Build both OPC UA libraries
and run `xmake test` before committing an update.

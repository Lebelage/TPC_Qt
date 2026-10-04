# TPC Slint architecture

## Dependency direction

```text
Slint components (ui/app.slint)
    -> ApplicationPresenter (src/ui)
        -> Application services
            -> Domain models
            -> TPC_API
            -> File system
```

The `.slint` layer contains presentation and interaction declarations only.
The generated Slint API is isolated in `ApplicationPresenter`, which translates
UI callbacks and background service events. `main.cpp` only composes the
context, windows, and presenter. Domain models and services do not include
Slint types.

## Composition and lifetime

`application::ApplicationContext` is the composition root. It owns the event
dispatcher, persistence, and TPC/field services in dependency order. The
presenter owns UI event subscriptions and is destroyed before the context.

Initial settings are loaded after every subscriber is registered. Background
TPC and slice-rendering workers are stopped before the event dispatcher is
destroyed. Worker callbacks use `slint::invoke_from_event_loop` before touching
generated UI objects.

## Source areas

- `ui`: declarative Slint presentation, reusable controls, and theme tokens.
- `src/application`: process composition and application lifetime.
- `src/models`: toolkit-independent application and domain data.
- `src/services`: application workflows, persistence, TPC integration, and
  field-slice rendering.
- `src/ui`: the narrow adapter between generated Slint types and services.
- `src/main.cpp`: minimal process entry point.

## TPC_API boundary

`TpcService.cpp` is the adapter for the external TPC library. Other application
areas do not include TPC system or analytics implementation headers. The
adapter consumes `<tpc/tpc.hpp>` and translates backend events into
application-owned event types.

Development builds use the bundled checkout selected by `TPC_API_DIR`. Release
and CI builds may use `find_package(TPC CONFIG)` by setting
`TPC_SLINT_USE_INSTALLED_TPC=ON`. Both modes link the public `TPC::TPC` target.

## Settings

Settings use the platform configuration directory:

- macOS: `~/Library/Application Support/TPC/TPC_Slint/settings.json`;
- Windows: `%APPDATA%/TPC/TPC_Slint/settings.json`;
- Linux: `$XDG_CONFIG_HOME/TPC_Slint/settings.json` or
  `~/.config/TPC_Slint/settings.json`.

On first launch, `Settings/settings.json` is copied to the new location when it
exists. Packaged applications do not write beside the executable.

## Build and packaging

Slint markup is compiled ahead of time by `slint_target_sources`. CMake first
uses an installed Slint 1.18 package and otherwise fetches the pinned 1.18.1
source release. The fallback needs Rust 1.92 or newer.

Installable artifacts are produced with:

```text
cmake --install <build-directory> --prefix <staging-directory>
```

# TPC Slint architecture

## Dependency direction

```text
Slint components (ui/app.slint)
    -> ApplicationViewBinding (src/ui)
        -> ApplicationViewModel (src/viewmodels)
            -> Application services
                -> Domain models
                -> TPC_API
                -> File system
```

The `.slint` layer is the View and contains presentation and interaction
declarations only. `ApplicationViewModel` owns screen state, validation, and
commands without depending on Slint. The generated Slint API is isolated in
`ApplicationViewBinding`, a thin adapter that observes ViewModel state
and forwards UI callbacks. `main.cpp` only composes the context, ViewModel,
windows, and binding adapter. Domain models and services do not include Slint
types.

Screen snapshots share immutable sensor rows. Incoming frames use an identity
index and format only changed measurement components; a new row snapshot is
published only when the displayed values change. The Slint adapter keeps its
table model, updates changed rows in place, and uploads slice pixels only when
the rendered slice changes. ViewModel state is protected by a mutex; observer
notifications and service calls happen after that mutex is released.

## Composition and lifetime

`application::ApplicationContext` is the composition root. It owns the event
dispatcher, persistence, and TPC/field services in dependency order. The
ViewModel owns service subscriptions. The binding adapter owns ViewModel
subscriptions, and both are destroyed before the context.

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
- `src/viewmodels`: toolkit-independent observable screen state and commands.
- `src/ui`: the narrow adapter between generated Slint types and ViewModels.
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

Settings are stored with the application:

- all platforms: `Settings/settings.json` next to the application executable.

On first launch, `Settings/settings.json` is created with default values. If it
is empty, malformed, or cannot be deserialized, it is replaced with defaults.

## Build and packaging

Slint markup is compiled ahead of time by `slint_target_sources`. CMake first
uses an installed Slint 1.18 package and otherwise fetches the pinned 1.18.1
source release. The fallback needs Rust 1.92 or newer.

Installable artifacts are produced with:

```text
cmake --install <build-directory> --prefix <staging-directory>
```

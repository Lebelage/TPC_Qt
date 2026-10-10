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

The API lives in `lib/tpc` and is built as the `tpc` static library.
The application services form `tpc_app`, shared by the desktop target and tests.
OPC UA sources live in `third_party` and are compiled directly by xmake.

## Settings

Settings are stored with the application:

- all platforms: `Settings/settings.json` next to the application executable.

On first launch, `Settings/settings.json` is created with default values. If it
is empty, malformed, or cannot be deserialized, it is replaced with defaults.

## Build and packaging

Slint 1.18.1 is fetched and built as a local xmake package. Its official
CMake/Corrosion build uses Cargo to build the C++ runtime and UI compiler from
one source release. Rust 1.92+ is required; CMake and Ninja are supplied by xrepo.
The package provides SDK paths and linkage to the desktop target only.
The `slint` rule generates C++ sources, embeds UI resources and packages
the shared runtime alongside the application.
No externally installed Slint SDK or compiler is used.

Use `xmake f --app=n` to build libraries and tests without the Slint SDK.

Installable artifacts are produced with:

```sh
xmake f -m release
xmake
xmake install -o out/release TPC_Slint
```

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

Typography, spacing and palette tokens live in `ui/theme.slint`; shared controls
and text styles live in `ui/components.slint`. Both windows and standard widgets
use the same Fluent dark style selected in `xmake.lua`. The scientific plot keeps
a light data surface with the same typography. `ui/sensor-table.slint` owns the
virtualized sensor list and its fixed header, synchronized on horizontal scroll.
`ui/settings-page.slint` provides a vertically scrollable form, and
`ui/diagnostics-panel.slint` keeps long quality messages readable without resizing
the table.

The field plot is split into `ui/field-window.slint` (window controls and readout)
and `ui/field-map.slint` (plot presentation and camera interaction). Shared controls
live in `ui/components.slint`; `ui/app.slint` exports both application windows.
`field_map_overlay.cpp` computes physical grid ticks, colour-scale labels and Hall
sensor projections without Slint types. The binding updates the grid on camera
changes, and recreates sensor/colour models only when their source data changes.
`RenderedFieldSlice` contains the immutable raster and numeric sample data;
`FieldSliceService` manages requests and background worker lifetime.

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
The application services and toolkit-independent ViewModels form `tpc_app`,
shared by the desktop target and tests.
OPC UA sources live in `third_party` and are compiled directly by xmake.

## Settings

Settings are stored with the application:

- all platforms: `Settings/settings.json` next to the application executable.

On first launch, a missing or empty default file is initialized with defaults.
Malformed settings are reported without overwriting the file. Users can load
another JSON profile from Settings; explicit loads are transactional and never
fall back to defaults. Apply writes to the active file. See
[Configuration profiles](configuration-profiles.md) for the schema and merge rules.

Server discovery merges configured and discovered identities without deleting
file-only sensors. Availability requires all R/F/Z channels in the current
session, while `enabled` stores the user's independent selection. Only available,
enabled sensors with known positions enter reconstruction. Discovery state and
monotonic publication revisions are never serialized; revisions prevent stale
worker notifications from undoing a newer selection. Per-channel calibration
from a profile overrides the built-in collection, with built-in fallback for
unspecified channels.

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

# TPC Qt architecture

## Dependency direction

```text
QML views
    -> View models
        -> Application services
            -> Domain models
            -> TPC_API
            -> File system / Qt dialogs
```

Dependencies must not point back toward QML. Domain models must not include Qt
types. View models may expose Qt types, but do not perform file, network, or
field-calculation work themselves.

## Composition and lifetime

`application::ApplicationContext` is the composition root. It owns the event
dispatcher, persistence, services, and view models in dependency order. New
long-lived services must be constructed there and passed explicitly to their
consumers. Do not add process-wide `instance()` accessors.

Initial settings are loaded only after all subscribers have been constructed.
Background TPC workers are stopped before the event dispatcher and view models
are destroyed.

## Source areas

- `src/application`: process composition and application lifetime.
- `src/models`: Qt-independent application/domain data where possible.
- `src/services`: application workflows and infrastructure adapters.
- `src/viewmodel`: state and commands exposed to QML.
- `qml`: presentation only; no persistence, network, or analytics logic.

As the project grows, services should be split by responsibility rather than
forming one large controller. In particular, field visualization should use a
dedicated field-slice service and view model instead of expanding
`WorkspaceViewModel` indefinitely.

## TPC_API boundary

`TpcService.cpp` is the adapter for the external TPC library. Application
models and view models must not include `tpc/system/*` or analytics headers.
The adapter consumes the public `<tpc/tpc.hpp>` header and translates backend
events into application-owned event types.

Meson first tries an installed `TPC` package and otherwise obtains the pinned
`TPC_API` revision through `subprojects/tpc_api.wrap`. The local Meson overlay
builds the library and exposes it only through the `tpc_dep` dependency.

## Settings

Settings are stored below `QStandardPaths::AppConfigLocation`. On first launch,
the legacy `Settings/settings.json` file is copied to the new location when it
exists. Packaged applications must not write beside the executable.

## Packaging

The normal build target is intended for development. A self-contained bundle
is produced via:

```text
meson compile -C <build-directory> deploy
```

The deploy target runs the platform Qt deployment tool and copies the required
runtime libraries, QML imports, and plugins. Release archives and installers
must be created from the deployed application, not the development executable.

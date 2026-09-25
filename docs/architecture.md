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

Development builds may add a `TPC_API` source checkout with `TPC_API_DIR`.
Release and CI builds may consume an installed package with `find_package(TPC
CONFIG)` by setting `TPC_QT_USE_INSTALLED_TPC=ON`. Both modes link only the
public `TPC::TPC` target.

## Settings

Settings are stored below `QStandardPaths::AppConfigLocation`. On first launch,
the legacy `Settings/settings.json` file is copied to the new location when it
exists. Packaged applications must not write beside the executable.

## Packaging

The build tree is for compilation only. A deployable directory is produced via:

```text
cmake --install <build-directory> --prefix <staging-directory>
```

Qt's generated deployment script copies the required runtime libraries, QML
imports, and plugins into the staging directory. Release archives and installers
must be created from that directory, never from the build tree.

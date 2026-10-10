# TPC Slint

Приложение для управления TPC и расчёта магнитного поля. C++23, Slint и xmake.

## Организация сборки

```text
xmake.lua                 единая конфигурация приложения, библиотек и тестов
xmake/slint.lua           сборка Slint из исходников, генерация UI и runtime
lib/tpc/                  собственная библиотека TPC API
third_party/              исходники open62541 и open62541pp с лицензиями
third_party/xmake.lua     прямая сборка сторонних библиотек через xmake
src/                      приложение: сервисы, модели, ViewModel и UI-привязки
ui/                       интерфейс Slint
tests/                    тесты приложения
docs/                     документация
```

Все цели описаны одной системой. `TPC_Slint` использует `tpc_app`, которая
использует `tpc`; далее идут `open62541pp` и `open62541`. Тесты используют
те же собранные библиотеки — исходники сервисов не компилируются повторно
для каждого теста. TPC API перенесена из `subprojects/tpc_api` в `lib/tpc`.

| Зависимость | Подключение |
| --- | --- |
| Eigen 5.0.1 | xrepo |
| stdexec 2026.07.06 | xrepo |
| nlohmann_json 3.12.0 | xrepo |
| open62541 1.4.20 | исходники в `third_party`, статическая библиотека xmake |
| open62541pp 0.21.3 | исходники в `third_party`, статическая библиотека xmake |
| Slint C++ SDK 1.18.1 | локальный пакет xmake, сборка из исходников |

Зависимости xrepo скачиваются автоматически; версии закреплены в конфигурации
и `xmake-requires.lock`. OPC UA библиотеки входят в репозиторий, не скачиваются
при сборке и не используют CMake. Подробности происхождения и обновления —
[third_party/README.md](third_party/README.md).

## Требования

- xmake 3.0+;
- компилятор C++23: Apple Clang на macOS или MSVC из Visual Studio 2022 на Windows;
- Rust 1.92+ через [rustup](https://rustup.rs/), `cargo` и `rustc` в `PATH`;
- сеть при первой установке зависимостей и Rust crates.

## Сборка

Slint 1.18.1 скачивается и собирается автоматически как локальный пакет xmake.
Официальная сборка Slint через CMake/Corrosion вызывает Cargo и создаёт C++
библиотеку, заголовки и `slint-compiler` одной версии. CMake и Ninja устанавливает
xrepo; сборка проекта и управление всеми зависимостями остаются в xmake.
Устанавливать Slint через brew, установщик или отдельный `cargo install` не нужно.

```sh
rustc --version
cargo --version
xmake f -m debug
xmake
xmake run TPC_Slint
```

На Windows используйте Developer PowerShell Visual Studio и Rust toolchain
`stable-x86_64-pc-windows-msvc`. Для Clang можно добавить `--toolchain=clang-cl`
к команде конфигурации xmake.

Первая конфигурация занимает больше времени: собираются Slint и Rust-зависимости.
Готовый SDK хранится в кеше пакетов xmake и используется повторно.

## Release и установка

```sh
xmake f -m release
xmake
xmake install -o out/release TPC_Slint
```

Release включает оптимизацию размера и LTO. На macOS устанавливается
`out/release/TPC_Slint.app` с runtime внутри bundle; на Windows —
`out/release/bin/TPC_Slint.exe` с DLL. Архив/установщик не создаётся.

## Библиотеки и тесты без GUI

```sh
xmake f -m debug --app=n
xmake tpc
xmake test
```

В этом режиме Slint и Rust не требуются. `xmake test` собирает и запускает
научные проверки, тесты логирования и тест кадра API. Для возврата к приложению
используйте `xmake f -m debug --app=y`.

## IDE и локальные файлы

После сборки автоматически обновляется `compile_commands.json`.
В CLion используйте поддержку xmake либо Compilation Database.
`build/`, `.xmake/`, `out/` и база компиляции игнорируются Git.
Исходники `lib/tpc` и `third_party` коммитятся вместе с приложением;
Git-подмодулей и дополнительных шагов загрузки нет.

## Документация

- [Архитектура](docs/architecture.md)
- [Логирование](docs/logging.md)
- [Научная валидация](docs/scientific-validation.md)

# TPC Slint

Приложение для управления TPC и расчёта магнитного поля. C++23, Slint и xmake.

## Организация сборки

```text
xmake.lua                 единая конфигурация приложения, библиотек и тестов
xmake/slint.lua           подключение установленного SDK, генерация UI и runtime
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
| Slint C++ SDK 1.18+ | установленный SDK |

Зависимости xrepo скачиваются автоматически; версии закреплены в конфигурации
и `xmake-requires.lock`. OPC UA библиотеки входят в репозиторий, не скачиваются
при сборке и не используют CMake. Подробности происхождения и обновления —
[third_party/README.md](third_party/README.md).

## Требования

- xmake 3.0+;
- компилятор C++23: Apple Clang на macOS или MSVC из Visual Studio 2022 на Windows;
- готовый Slint C++ SDK для нужной платформы и архитектуры;
- сеть при первой установке пакетов xrepo.

Slint не скачивается проектом. SDK должен содержать компилятор UI, заголовки
и runtime одной версии. Rust для сборки приложения не нужен.

## Сборка

Slint устанавливается разработчиком. Сборка находит `slint-compiler` в `PATH`
и подключает заголовки и библиотеки из того же SDK. На macOS библиотека также
находится через `brew --prefix slint-cpp`.

Homebrew устанавливает только C++ библиотеку, без компилятора UI:

```sh
brew install slint-cpp
```

Отдельно установите `slint-compiler` той же версии из
[релизов Slint](https://github.com/slint-ui/slint/releases) и добавьте каталог
с бинарником в `PATH`. Если установлен Rust, для Slint 1.18.1 можно использовать:

```sh
cargo install --git https://github.com/slint-ui/slint --tag v1.18.1 --locked slint-compiler
export PATH="$HOME/.cargo/bin:$PATH"
```

Для полного SDK в другом каталоге добавьте его `bin`:

```sh
export PATH="/путь/к/slint-sdk/bin:$PATH"
xmake f -m debug
xmake
xmake run TPC_Slint
```

В Windows установите C++ SDK через установщик Slint, добавьте его каталог `bin`
в переменную среды `PATH` и используйте Developer PowerShell Visual Studio:

```powershell
$env:PATH = "C:\SDK\Slint\bin;$env:PATH"
xmake f -m debug
xmake
xmake run TPC_Slint
```

Специальных параметров xmake для Slint нет. Если SDK не найден или неполон,
конфигурация завершается с сообщением об ошибке.

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

В этом режиме Slint SDK не требуется. `xmake test` собирает и запускает
научные проверки, тесты логирования и тест кадра API. Для возврата к приложению
используйте `xmake f -m debug --app=y` с установленным SDK.

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

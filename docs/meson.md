# Сборка Meson + vcpkg

Основной сценарий сборки — Meson. CMake-файл в `subprojects/tpc_api` относится
только к библиотеке TPC и не является второй системой сборки приложения.

## Зависимости

Проект использует установленный checkout vcpkg и ничего не скачивает во время
конфигурации Meson. Корень vcpkg выбирается в таком порядке:

1. параметр Meson `-Dvcpkg_root=/path/to/vcpkg`;
2. исполняемый файл `vcpkg` (`vcpkg.exe`) из `PATH`;
3. переменная окружения `VCPKG_ROOT`.

Qt 6, Eigen3, nlohmann-json, open62541pp и stdexec ищутся сначала в переносимом
каталоге `vcpkg_installed/<triplet>` внутри проекта, затем в
`<VCPKG_ROOT>/installed/<triplet>`. Другой каталог можно передать параметром
`-Dvcpkg_install_root=/path/to/packages`. Для Qt host-инструменты должны
находиться в triplet текущей машины. Если checkout, целевой triplet, пакет,
библиотека или Qt-инструмент отсутствуют, `meson setup` завершается понятной
ошибкой. Системная Qt и случайно установленные CMake-пакеты намеренно не
подхватываются.

Список зависимостей хранится в `vcpkg.json`. Установка в локальный каталог на
Windows x64 из PowerShell:

```powershell
$env:VCPKG_ROOT = "C:\Tools\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows `
  --x-install-root="$PWD\vcpkg_installed"
```

На macOS следует использовать project triplet: он сохраняет корректные
framework paths Qt и исправляет rpath host-инструментов:

```sh
export VCPKG_ROOT="$HOME/Tools/vcpkg"
"$VCPKG_ROOT/vcpkg" install --triplet arm64-osx \
  --overlay-triplets="$PWD/meson/triplets" \
  --x-install-root="$PWD/vcpkg_installed"
```

Для Intel Mac замените `arm64-osx` на `x64-osx`. Каталог `vcpkg_installed`
игнорируется Git: при переносе исходников зависимости устанавливаются заново
для целевой ОС.

## Профили Debug и Release

У Meson нет файла `CMakePresets.json`. Его штатный переносимый аналог для этих
настроек — native files, которые лежат в проекте:

- `meson/profiles/debug.ini` — символы отладки, без оптимизации, assertions
  включены, debug-библиотеки Qt;
- `meson/profiles/release.ini` — оптимизация Release, `NDEBUG`, release-библиотеки
  Qt.

ОС и архитектура намеренно не записаны в профиле. Их задаёт toolchain: на Mac
получается macOS-сборка, на Windows — Windows-сборка. Это не кросс-компиляция
между ОС.

```sh
meson setup .build/debug --native-file meson/profiles/debug.ini
meson compile -C .build/debug

meson setup .build/release --native-file meson/profiles/release.ini
meson compile -C .build/release
```

Для каждого профиля нужен свой build-каталог. Целевой архив создаётся командой
`meson compile -C .build/release package`.

## CLion

CLion пока не создаёт переключатель из нескольких Meson native files, как для
CMake Presets, но напрямую поддерживает параметр `--native-file`.

Откройте корень проекта с `meson.build`, затем в
`Settings | Build, Execution, Deployment | Meson` задайте:

- **Debug:** Setup options — `--native-file=meson/profiles/debug.ini`, Build
  directory — `.build/clion-debug`;
- **Release:** Setup options — `--native-file=meson/profiles/release.ini`, Build
  directory — `.build/clion-release`.

После смены профиля выполните `Tools | Meson | Wipe and Reload Meson Project`.
На Windows выберите Visual Studio toolchain нужной архитектуры (`amd64` или
`arm64`) и запускайте CLion в окружении, где доступен `VCPKG_ROOT`. На macOS
используйте системный Apple Clang toolchain. CLion сам обнаружит executable
`TPC_Qt` и создаст Native Application run/debug configuration.

## VS Code

Откройте `Terminal -> Run Build Task` и выберите одну из четырёх основных задач:

- `Meson: build Windows Debug`;
- `Meson: build Windows Release`;
- `Meson: build macOS Debug`;
- `Meson: build macOS Release`.

Windows-задача дополнительно спрашивает архитектуру `x86_64` или `arm64`.
Конфигурация выполняется автоматически перед сборкой, а после неё запускается
`deploy`, поэтому результат сразу готов к запуску. Для Release-архива используйте
`Meson: package Windows` или `Meson: package macOS`.

Для обычного запуска без отладчика выполните `Tasks: Run Task` и выберите
`Meson: run <платформа> Debug` либо `Meson: run <платформа> Release`. Задача сама
соберёт и развернёт приложение перед запуском.

В разделе `Run and Debug` оставлено по одному Debug и Release профилю на ОС.
Windows-профиль также спрашивает целевую архитектуру. Запуск автоматически
вызывает соответствующую задачу сборки. `F5` запускает отладку, `Ctrl+F5`
(`Control+F5` на macOS) запускает тот же профиль без отладчика.

На Windows запускайте VS Code из Developer PowerShell/Command Prompt Visual
Studio, настроенного на выбранную целевую архитектуру. Нужны C++ workload и
Windows SDK. На macOS сборка использует нативную архитектуру машины; нужны Xcode
Command Line Tools, Meson, Ninja, CMake и утилиты, необходимые портам vcpkg.

## Командная строка

macOS, нативная Debug-сборка:

```sh
meson setup .build/meson-macos-native-debug --native-file meson/profiles/debug.ini
meson compile -C .build/meson-macos-native-debug deploy
```

Windows x64, Release-сборка:

```powershell
meson setup .build/meson-windows-x86_64-release `
  --native-file meson/profiles/release.ini `
  --cross-file meson/cross/windows-x86_64.ini
meson compile -C .build/meson-windows-x86_64-release deploy
meson compile -C .build/meson-windows-x86_64-release package
```

Повторный `meson setup` безопасен: Meson сообщает, что каталог уже настроен, а
изменения файлов сборки подхватываются при `meson compile`. Для явной повторной
конфигурации можно добавить `--reconfigure`.

Сборка Windows выполняется на Windows, macOS — на macOS. Machine-файлы Windows
переключают целевую архитектуру, но не предоставляют компилятор другой ОС.

`deploy` создаёт `.app` на macOS или копирует DLL и QML рядом с `.exe` на
Windows. `package` доступен только для Release и создаёт ZIP в `dist/`. QML
включается в приложение через QRC; `qmlcachegen` отдельно не запускается.

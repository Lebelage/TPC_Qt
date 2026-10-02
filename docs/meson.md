# Сборка Meson + vcpkg

Основной сценарий сборки — Meson. CMake-файл в `subprojects/tpc_api` относится
только к библиотеке TPC и не является второй системой сборки приложения.

## Зависимости

Проект использует уже установленный checkout vcpkg и ничего не скачивает и не
устанавливает во время конфигурации. Корень vcpkg выбирается в таком порядке:

1. параметр Meson `-Dvcpkg_root=/path/to/vcpkg`;
2. исполняемый файл `vcpkg` (`vcpkg.exe`) из `PATH`;
3. переменная окружения `VCPKG_ROOT`.

Qt 6, Eigen3, nlohmann-json, open62541pp и stdexec должны находиться в
`<VCPKG_ROOT>/installed/<triplet>`. Для Qt host-инструменты должны находиться в
triplet текущей машины. Если checkout, целевой triplet, пакет, библиотека или
Qt-инструмент отсутствуют, `meson setup` завершается ошибкой. Системная Qt и
случайно установленные CMake-пакеты намеренно не подхватываются.

Список зависимостей хранится в `vcpkg.json`. При их установке для macOS следует
использовать triplet-файлы из `meson/triplets`: они сохраняют корректные
framework paths Qt и исправляют rpath host-инструментов. Установка зависимостей
остаётся отдельной явной операцией vcpkg.

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
meson setup .build/meson-macos-native-debug --buildtype=debug
meson compile -C .build/meson-macos-native-debug deploy
```

Windows x64, Release-сборка:

```powershell
meson setup .build/meson-windows-x86_64-release --buildtype=release `
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

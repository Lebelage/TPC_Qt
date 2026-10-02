# Сборка Meson + vcpkg

Проект рассчитывает на установленные компилятор, Meson, Ninja, CMake, Git и vcpkg.
Бинарник vcpkg должен быть доступен через `PATH`. Полный checkout можно указать
через `VCPKG_ROOT` или `-Dvcpkg_root=/path/to/vcpkg`. Если установлен только
бинарник (так работает формула Homebrew), Meson сам загружает служебный checkout
нужной версии в `.build/vcpkg-root`. Отдельная команда настройки не требуется.

```sh
meson setup build --buildtype=debug
meson compile -C build
meson compile -C build deploy
```

При настройке Meson запускает `vcpkg install` по `vcpkg.json`, включая закреплённый
baseline. Зависимости хранятся в `.build/meson-vcpkg/<triplet>` и используются
совместно разными каталогами сборки. Уже установленные пакеты повторно не собираются;
vcpkg использует свой бинарный кеш. Изменение манифеста запускает перенастройку.
Qt, Eigen, nlohmann-json, open62541pp и stdexec берутся из этого каталога.
Установленная отдельно системная Qt не используется.
Исходники TPC_API закреплены в `subprojects/tpc_api.wrap`.

На macOS нужны Xcode Command Line Tools и инструменты, необходимые портам vcpkg:
`pkg-config`, `autoconf`, `automake`, `libtool` и `autoconf-archive`, а также Python 3.
На Windows выполняйте команды из подходящего Developer Command Prompt / PowerShell
Visual Studio с C++ workload и Windows SDK. VS Code также запускайте из этого окружения.
Для ARM64 используйте ARM64 toolchain и
`--cross-file meson/cross/windows-arm64.ini`; для x64 — соответствующий x64 файл.
Архитектура triplet определяется целевой машиной Meson.

Для Release:

```sh
meson setup build-release --buildtype=release
meson compile -C build-release package
```

`deploy` создаёт `TPC_Qt.app` рядом с исполняемым файлом на macOS и копирует DLL/QML
рядом с `.exe` на Windows. `package` доступен для Release и создаёт приложение
с зависимостями и ZIP в `dist/`.

Задачи VS Code вызывают Meson напрямую. `configure` можно повторять;
`build` также выполняет `deploy`, чтобы приложение было готово к запуску отладчика.
После перехода со старой настройки создайте новый каталог сборки: старые machine files
больше не нужны.

Остались только скрипты упаковки и исправление Qt tools на macOS.
Последнее выполняется как hook порта vcpkg: после перемещения инструментов Qt
исправляет их Mach-O rpath и подпись, чтобы moc/rcc и другие инструменты запускались.
Meson создаёт `qt.conf` и копию macdeployqt в каталоге сборки, чтобы упаковка использовала
правильные Debug/Release плагины. На Windows аналогично копируется qtpaths.exe
для инструмента windeployqt. Эти файлы генерируются автоматически и не требуют запуска.
QML включается в приложение через QRC на обеих платформах; предварительная компиляция
QML через qmlcachegen не выполняется, Qt обрабатывает QML при запуске.

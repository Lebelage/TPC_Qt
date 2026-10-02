# Сборка Meson + vcpkg

Проект рассчитывает на заранее установленные компилятор, Meson, Ninja, CMake и
полный checkout vcpkg. Meson выбирает его через `-Dvcpkg_root`, затем
ищет `vcpkg` или `vcpkg.exe` в `PATH`, и только затем использует системный
`VCPKG_ROOT` как запасной вариант. Поэтому Developer Environment Visual Studio
не перехватывает выбор, если в `PATH` уже есть другой vcpkg. Корнем считается
каталог рядом с исполняемым файлом. Отдельная команда настройки проекта не
требуется, а сам vcpkg проект не клонирует и не обновляет.

```sh
meson setup build --buildtype=debug
meson compile -C build
meson compile -C build deploy
```

Meson не запускает `vcpkg install`, ничего не скачивает и не пересобирает. Файл
`vcpkg.json` описывает необходимые зависимости, а готовые библиотеки берутся из
`<VCPKG_ROOT>/installed/<triplet>`. Если пакет, нужный feature или host-инструмент
отсутствует, конфигурация завершится ошибкой с именем недостающей зависимости.
Qt, Eigen, nlohmann-json, open62541pp и stdexec должны быть заранее установлены в
выбранном vcpkg. Установленная отдельно системная Qt не используется.
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

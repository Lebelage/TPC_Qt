# TPC Slint

Приложение для управления TPC и расчёта магнитного поля. Интерфейс — Slint,
прикладная логика и встроенная библиотека TPC API — C++23.
Историческое имя репозитория — `TPC_Qt`, имя приложения — `TPC_Slint`.

## Структура проекта

```text
src/                          приложение, сервисы, модели и привязки UI
ui/                           интерфейс Slint
subprojects/tpc_api/           встроенная библиотека TPC API
tests/                        проверки приложения
docs/                         архитектура, логирование и валидация
CMakeLists.txt                зависимости и цели сборки
CMakePresets.json             общие настройки платформ и режимов
CMakeUserPresets.json          личные настройки; игнорируются Git
CMakeUserPresets.json.example  пример личных настроек для macOS
vcpkg.json                    зависимости и baseline vcpkg
.build/                       результаты сборки; игнорируются Git
out/                          установленное приложение; игнорируется Git
```

## Требования

- CMake 3.25 или новее;
- Ninja в `PATH`;
- компилятор C++23: Apple Clang на macOS или Visual Studio 2022 на Windows;
- Rust 1.92 или новее, если нет установленного Slint C++ SDK;
- vcpkg, содержащий baseline из `vcpkg.json`.

CMake сначала ищет установленный Slint 1.18. Если он отсутствует, скачивает
и собирает Slint 1.18.1. Для этого нужны `rustc` и `cargo` в `PATH`.
Первая сборка требует доступа к сети для загрузки зависимостей.

## Скачивание и обновление

Скачайте нужную ветку; исходники TPC API входят в тот же репозиторий:

```sh
git clone --branch main https://github.com/Lebelage/TPC_Qt.git
cd TPC_Qt
```

`subprojects/tpc_api` — обычная папка в Git. Коммитьте и отправляйте её изменения
вместе с приложением. Отдельное скачивание API и инициализация подмодулей
не требуются. Не создавайте вложенный `.git` в этой папке.

Для обновления чистой рабочей копии используйте `git pull --ff-only`.
Свои изменения предварительно сохраните коммитом или через `git stash`.

### Обновление старой копии с API-подмодулем на Windows

Перед обновлением сохраните старую папку API вместе с локальными изменениями
и вложенным `.git`. Из корня проекта в PowerShell:

```powershell
$backupName = 'tpc_api_backup-' + [Guid]::NewGuid().ToString('N')
Rename-Item -LiteralPath .\subprojects\tpc_api -NewName $backupName
git pull --ff-only
cmake --preset windows-release
cmake --build --preset windows-package
```

Резервная папка остаётся на диске и игнорируется Git. Сравните её изменения
с новой API перед удалением. Сначала сохраните локальные изменения приложения;
обновляйтесь после публикации встроенной API в выбранной ветке.
`git submodule update` больше не требуется.

## Сборка на macOS

Укажите путь к своему vcpkg. Ninja ищется через `PATH`; пресет также учитывает
каталоги Homebrew для Apple Silicon и Intel:

```sh
export VCPKG_ROOT="$HOME/Tools/vcpkg"
cmake --preset debug
cmake --build --preset debug
```

## Сборка на Windows

Задайте `VCPKG_ROOT` до запуска CLion. Для командной строки используйте
Developer PowerShell с компилятором Visual Studio:

```powershell
$env:VCPKG_ROOT = "C:\src\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
```

## Готовое приложение

На macOS:

```sh
cmake --preset release
cmake --build --preset package
```

Результат: `out/release/TPC_Slint.app`. На Windows:

```powershell
cmake --preset windows-release
cmake --build --preset windows-package
```

Результат: `out/windows-release/bin/TPC_Slint.exe` с зависимыми DLL.
Пресеты `package` запускают CMake install; архив или установщик они не создают.

## Локальные настройки и CLion

Создайте `CMakeUserPresets.json` из `CMakeUserPresets.json.example` и укажите
свой `VCPKG_ROOT`. Пример рассчитан на macOS с vcpkg в `~/Tools/vcpkg`.
Если путь подходит, личный файл можно сократить до:

```json
{
  "version": 6,
  "include": ["CMakeUserPresets.json.example"]
}
```

В CLion откройте корневой проект, выберите пресет `local-debug` или
`local-release` и цель `TPC_Slint`. API отдельно открывать не требуется.
Локальные пресеты не зависят от `VCPKG_ROOT` в окружении IDE:

```sh
cmake --preset local-debug
cmake --build --preset local-debug
cmake --preset local-release
cmake --build --preset local-package
```

Они используют каталоги `.build/debug`, `.build/release` и `out/release`,
как основные macOS-пресеты. Запускайте основные и локальные сборки по очереди.
Для Windows личный пресет должен наследовать `windows-debug` или
`windows-release`, а `VCPKG_ROOT` содержать Windows-путь.

Общий `CMakePresets.json` хранится в Git; личный `CMakeUserPresets.json`
игнорируется. При смене компилятора или vcpkg используйте новый `binaryDir`,
чтобы не переиспользовать несовместимый CMake cache.

## Организация пресетов

| Платформа | Debug | Release | Установка Release |
| --- | --- | --- | --- |
| macOS | `debug` | `release` | `package` |
| Windows | `windows-debug` | `windows-release` | `windows-package` |

Общие пути и зависимости заданы в `common`, настройки платформ — в `macos`
и `windows`, режимов — в `debug-mode` и `release-mode`. Это скрытые пресеты,
которые не нужно запускать отдельно. Debug включает символы и
`compile_commands.json`; Release — оптимизацию размера и LTO.

## Подключение TPC API

По умолчанию используется `subprojects/tpc_api`. Для другой копии исходников
передайте `-DTPC_API_DIR=/путь/к/api` при конфигурации. Для установленного пакета
используйте `-DTPC_SLINT_USE_INSTALLED_TPC=ON` вместе с путём поиска пакета.

## Документация

- [Архитектура](docs/architecture.md)
- [Логирование](docs/logging.md)
- [Научная валидация](docs/scientific-validation.md)

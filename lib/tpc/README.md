# TPC API

Собственная библиотека C++23 для связи по OPC UA и расчёта магнитного поля.
Публичный заголовок — `<tpc/tpc.hpp>`, цель xmake — `tpc`.

Из корня репозитория:

```sh
xmake f -m debug --app=n
xmake tpc
xmake test
```

Конфигурация библиотеки находится в корневом `xmake.lua`. Публичные include-пути
и зависимости передаются потребителям через `add_deps("tpc")`.
Eigen и stdexec устанавливает xrepo; open62541pp/open62541 собираются
из исходников `third_party`. Slint для этой библиотеки не нужен.

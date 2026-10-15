# Редактирование и экспорт отчёта

Исходник LaTeX: `field-reconstruction-report.tex`. Текст, формулы и таблица
редактируются непосредственно в нём. Иллюстрации подключены из `screenshots/`:
`03-reconstruction.jpg`, `04-slice-z.jpg`, `05-slice-x.jpg`.
Они совпадают с изображениями, встроенными в HTML-отчёт.

## Сборка PDF

Требуется TeX Live / MacTeX с пакетами, перечисленными в преамбуле.
Исходник сохранён в UTF-8 и поддерживает pdfLaTeX, XeLaTeX и LuaLaTeX.
Для XeLaTeX/LuaLaTeX используются `polyglossia` и шрифт Libertinus Serif;
для pdfLaTeX — `babel`, кодировка T2A и кириллические шрифты LH.

Из корня проекта:

```sh
cd docs
xelatex -interaction=nonstopmode -halt-on-error field-reconstruction-report.tex
xelatex -interaction=nonstopmode -halt-on-error field-reconstruction-report.tex
```

Второй проход обновляет ссылку на рисунок. Результат:
`docs/field-reconstruction-report.pdf`.

Если редактор настроен на pdfLaTeX, используйте из каталога `docs`:

```sh
pdflatex -interaction=nonstopmode -halt-on-error field-reconstruction-report.tex
pdflatex -interaction=nonstopmode -halt-on-error field-reconstruction-report.tex
```

Преамбула автоматически выбирает пакеты для текущего движка.
Ошибка `fontspec: cannot-use-pdftex` означала, что прежний исходник,
требовавший XeLaTeX/LuaLaTeX, запускался через pdfLaTeX.

Если установлен `latexmk`, оба прохода можно выполнить одной командой из `docs`:

```sh
latexmk -xelatex -interaction=nonstopmode -halt-on-error field-reconstruction-report.tex
```

Для Overleaf загрузите `.tex` и папку `screenshots` с тремя указанными файлами,
назначьте этот исходник главным документом. Можно выбрать pdfLaTeX;
для шрифта Libertinus Serif выберите XeLaTeX или LuaLaTeX.

Расширенный отчёт рассчитан примерно на пять страниц: постановка и десять
базисных мод; обратная задача и SVD; опорная карта и диагностика;
результаты проверки; срезы и выводы. Формулы базиса и описание решения
сверены с `lib/tpc/include/tpc/analytics/models/basis_models.hpp`
и `lib/tpc/include/tpc/analytics/svd/svd_solver.hpp`.

Формат — A4, поля — 16 мм сверху и снизу, 18 мм слева и справа.
Каждый из пяти разделов начинается с новой страницы. Если текст станет длиннее,
LaTeX автоматически добавит страницы; для непрерывной вёрстки удалите команды
`\clearpage` перед разделами. Размеры снимков задаются в `\includegraphics`
и `minipage`. Подписи и ссылка на первый рисунок нумеруются автоматически.

HTML и Markdown сохранены как исходные версии. Изменения `.tex` не переносятся
в них автоматически.

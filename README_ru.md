# iGP11 proxychain

Прокси/загрузчик-DLL для [iGP11](https://www.nexusmods.com/darksouls3/mods/394)
(текстурный мод для Dark Souls 3 и похожих игр). Заменяет
`iGP11.Direct3D11.dll`, чтобы можно было:

* подгружать произвольный список доп. DLL (других модов, дебаг-хелперов и т.п.);
* патчить IAT игры — чтобы «пассивные» d3d11-style моды (например
  *DS3 ProperPCExperience*) действительно получали свой
  `D3D11CreateDevice`;
* при этом iGP11 продолжает работать как ни в чём не бывало.

> **Важно:** библиотека **без поддержки**, **навайбкожена с помощью
> Claude Opus 4.7**. Работает на сетапе автора, выложено as-is.
> Issues / PR'ы приветствуются, но никаких гарантий ответа, фиксов или
> совместимости с будущими версиями iGP11.
>
> *English version: see [README.md](README.md).*

## Зачем?

Я хотел запустить в Dark Souls 3 два мода одновременно:

* [**Keyboard Icons**](https://www.nexusmods.com/darksouls3/mods/294)
  — поставляется как набор подмен текстур для iGP11, то есть требует
  `iGP11.Direct3D11.dll`.
* [**DS3 Proper PC Experience Mod**](https://www.nexusmods.com/darksouls3/mods/1545)
  — поставляется как `d3d11.dll`: игра должна загрузить его как d3d11
  proxy и вызвать его экспорт `D3D11CreateDevice` (именно внутри него
  мод ставит свои хуки на DXGI — refresh rate, FPS cap, FOV, skip
  intro).

Из коробки эти два мода не уживаются вместе. iGP11 грузится своим
лаунчером через remote-thread инъекцию — но к этому моменту игра уже
давно резолвнула `d3d11.dll` через системную DLL, поэтому
`DS3_ProperPCExperience_Mod`, положенный как `d3d11.dll` в папку игры,
либо игнорируется, либо ломает iGP11 — зависит от порядка загрузки.
Сам лаунчер iGP11 тоже не умеет цеплять другие моды.

Этот проект и есть решение: прокси-DLL `iGP11.Direct3D11.dll`, которая
оставляет iGP11 рабочим **и** одновременно поднимает Proper PC
Experience (или любой другой пассивный d3d11-style мод), переписывая
IAT игрового EXE так, чтобы `D3D11CreateDevice` мода реально вызывался.

## Как это работает

iGP11.Tool инжектит `iGP11.dll` в игру, тот загружает
`iGP11.Direct3D11.dll` и зовёт его экспорты `get` / `start` / `stop`.

Наш `iGP11.Direct3D11.dll` встаёт вместо оригинала. В `DllMain` он:

1. Читает `iGP11.Direct3D11.dll.ini` рядом с собой.
2. `LoadLibrary` каждой DLL из секции `[DLLs]` в порядке файла.
3. Переписывает IAT игры по правилам из `[IAT_Hooks]`.
4. Загружает переименованный оригинал `iGP11_Direct3D11_real.dll`.
5. Форвардит `get` / `start` / `stop` ему — через forwarders в `.def`.

```
              ┌────────────────────────────────┐
              │ DarkSoulsIII.exe (host)        │
              │   imports d3d11.dll!D3D11Create│ ◄── IAT переписана ◄─┐
              └────────────────┬───────────────┘                      │
                  iGP11.Tool инжектит iGP11.dll                       │
                               │ LoadLibrary                          │
                               ▼                                      │
              ┌────────────────────────────────┐                      │
              │ iGP11.Direct3D11.dll (мы)      │                      │
              │   - читает .ini                │                      │
              │   - LoadLibrary мод-DLL ───────┼──► your_mod.dll ─────┘
              │   - патчит IAT игры            │
              │   - форвардит get/start/stop ─►│  ┌──────────────────────────────┐
              └────────────────────────────────┘  │ iGP11_Direct3D11_real.dll    │
                                                   │ (переименованный оригинал)   │
                                                   └──────────────────────────────┘
```

## Быстрый старт (без компилятора)

Скачай готовый DLL из
[Releases](https://github.com/k1zn/igp11-proxychain/releases),
или собери сам (см. [Сборка](#сборка)).

### 1. Подготовь файлы

В папке с iGP11 (например `D:\Games\DarkSoulsMods\iGP11\`):

| Действие | Файл |
|---|---|
| Переименуй | `iGP11.Direct3D11.dll` → `iGP11_Direct3D11_real.dll` (подчёркивания!) |
| Скопируй | `iGP11.Direct3D11.dll` (наш proxy) |
| Скопируй и переименуй | `iGP11.Direct3D11.dll.ini.example` → `iGP11.Direct3D11.dll.ini` |

### 2. Настрой `iGP11.Direct3D11.dll.ini`

Минимальный пример (грузим один мод, переписываем `D3D11CreateDevice`):

```ini
[Settings]
LogEnabled = 1

[DLLs]
1 = DS3_ProperPCExperience_Mod.dll

[IAT_Hooks]
1 = d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
```

### 3. Запусти игру

Через `iGP11.Launcher` как обычно. Рядом появится `proxychain.log`
с детальной картиной — каждый этап логируется.

## Конфиг

### `[Settings]`

| Ключ | По умолчанию | Что делает |
|---|---|---|
| `LogEnabled` | `1` | `0` отключает лог. |
| `LogFile` | `proxychain.log` | Путь (относительный к DLL или абсолютный). |
| `LoadOriginal` | `1` | Явно `LoadLibrary` переименованного оригинала. `0` оставит загрузку ОС-лоадеру через forwarders. |

### `[DLLs]`

Список DLL, грузятся в порядке строк файла. Ключи произвольные (главное —
уникальные внутри секции), значения — пути (абсолютные или относительные
к прокси-DLL).

### `[IAT_Hooks]`

Каждая строка переписывает один импорт игры на функцию в одной из
загруженных нами DLL:

```
<num> = target_dll!target_func -> resolver_dll!resolver_func
```

* `target_dll` / `target_func` — что игра импортирует (имя DLL
  регистронезависимо, имя функции — нет).
* `resolver_dll` — DLL из `[DLLs]` выше (то, что мы уже загрузили).
* `resolver_func` — её экспорт.

В логе появится либо `patched N IAT slot(s)`, либо
`0 IAT slots matched` — если хост вообще не импортирует эту функцию.

### Зачем нужен `[IAT_Hooks]`

Многие моды (refresh-rate-фиксеры, ENB, ReShade и т.п.) ставят свои хуки
**внутри** экспортированной функции `D3D11CreateDevice`. Если такой мод
просто `LoadLibrary` — его экспорт **никто не вызовет**, и хуки не
ставятся.

`[IAT_Hooks]` переписывает запись в IAT игры так, чтобы вызов
`d3d11.dll!D3D11CreateDevice` пошёл в мод. Мод ставит свои хуки и
форвардит вызов в системный `d3d11.dll`. В итоге iGP11 и мод работают
одновременно.

## Сборка

Понадобится:

* **MinGW-w64** под x86_64. Проще всего — портативный
  [w64devkit](https://github.com/skeeto/w64devkit/releases) (один ZIP,
  без инсталлятора).
* **Python 3**.

```sh
# открой w64devkit.exe, перейди в корень проекта

# 1. Сгенерируй forwarder .def под ТВОЮ копию iGP11.Direct3D11.dll.
#    В репо уже закоммичен src/proxy.def для стандартной iGP11; если у
#    тебя другая версия — перегенери.
python tools/gen_def.py /path/to/iGP11.Direct3D11.dll iGP11_Direct3D11_real src/proxy.def

# 2. Собери.
make
```

На выходе — `iGP11.Direct3D11.dll` в корне проекта.

### Своё имя для переименованного оригинала

Если хочешь использовать другое имя (по умолчанию
`iGP11_Direct3D11_real.dll`), перегенери `.def` с новым target'ом и
передай его в `make`:

```sh
python tools/gen_def.py /path/to/iGP11.Direct3D11.dll iGP11_orig src/proxy.def
make REAL_DLL=iGP11_orig.dll
```

ВАЖНО: в имени target'а **не должно быть точек** (кроме `.dll`) —
иначе forwarders не зарезолвятся. Windows-лоадер делит строку
`<dll>.<func>` по **первой** точке. Подробнее — в комментариях
`src/dllmain.cpp`.

## Структура проекта

```
.
├── README.md / README_ru.md
├── CLAUDE.md              # инструкция для AI-агентов в коде
├── LICENSE                # MIT
├── Makefile
├── iGP11.Direct3D11.dll.ini.example
├── src/
│   ├── dllmain.cpp        # DllMain, цикл [DLLs], цикл [IAT_Hooks]
│   ├── config.{h,cpp}     # INI парсер (GetPrivateProfileSectionW)
│   ├── log.{h,cpp}        # UTF-8 файловый лог (CreateFile/WriteFile)
│   ├── iat_patch.{h,cpp}  # обход импорт-таблицы PE, переписывание IAT
│   └── proxy.def          # forwarders (сгенерировано)
├── tools/
│   └── gen_def.py         # парсер PE → .def без зависимостей
└── .github/workflows/
    └── build.yml          # CI: сборка на каждом push
```

## Что делать, если…

| Симптом | Что не так |
|---|---|
| Игра падает сразу, в логе `LoadLibrary (original): -> FAILED` | Переименованного оригинала нет рядом или у него неверное имя. Дефолт — `iGP11_Direct3D11_real.dll`. |
| Для хука `0 IAT slots matched` | Хост EXE не импортирует эту функцию из этой DLL статически. Проверь через `objdump -p` или `dumpbin /imports`. |
| `proxychain.log` не появляется | `LogEnabled = 0` в INI или нет прав на запись. |
| Мод грузится, но не работает | Скорее всего, его хуки стоят внутри экспорта — добавь строку в `[IAT_Hooks]`, чтобы перенаправить вызов в этот экспорт. |
| Ошибка сборки `ERROR: src/proxy.def not found` | Сгенерируй: `python tools/gen_def.py ... src/proxy.def`. |

## Лицензия

MIT — см. [LICENSE](LICENSE). В этом проекте **нет** кода или
ассетов iGP11 или сторонних модов; мы лишь ссылаемся на их публичные
имена экспортов как на строки.

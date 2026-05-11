# Рецепт: Keyboard Icons + DS3 Proper PC Experience

Запустить [Keyboard Icons](https://www.nexusmods.com/darksouls3/mods/294)
и [DS3 Proper PC Experience Mod](https://www.nexusmods.com/darksouls3/mods/1545)
одновременно в Dark Souls 3.

> *English version: [keyboard-icons-and-proper-pc.md](keyboard-icons-and-proper-pc.md).*

## Что нужно

- DS3 уже работает через iGP11 (Keyboard Icons из коробки так и
  работает). Папка iGP11 — там, где у тебя лежит
  `iGP11.Direct3D11.dll`, обычно `...\iGP11\`. Дальше подставь свой
  путь.
- `iGP11.Direct3D11.dll` из последнего
  [релиза proxychain](https://github.com/k1zn/igp11-proxychain/releases).
- `d3d11.dll` из
  [DS3 Proper PC Experience Mod](https://www.nexusmods.com/darksouls3/mods/1545).

## Шаги

1. **Переименуй оригинальный iGP11 DLL** в папке iGP11:

   ```
   iGP11.Direct3D11.dll  →  iGP11_Direct3D11_real.dll
   ```

2. **Положи proxy.** Скопируй `iGP11.Direct3D11.dll` из zip-релиза
   proxychain в ту же папку.

3. **Переименуй Proper PC Experience** из `d3d11.dll` в
   `DS3_ProperPCExperience_Mod.dll` и положи туда же
   (переименование нужно чтобы не конфликтовать с системным
   `d3d11.dll`).

4. **Создай конфиг** `iGP11.Direct3D11.dll.ini` в этой же папке
   со следующим содержимым:

   ```ini
   [Settings]
   LogEnabled = 1

   [DLLs]
   1 = DS3_ProperPCExperience_Mod.dll

   [IAT_Hooks]
   1 = d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
   ```

Итог — рядом должны лежать четыре файла:

```
...\iGP11\
├── iGP11.Direct3D11.dll              ← proxychain (этот проект)
├── iGP11.Direct3D11.dll.ini          ← конфиг выше
├── iGP11_Direct3D11_real.dll         ← переименованный оригинал iGP11
└── DS3_ProperPCExperience_Mod.dll    ← переименованный Proper PC Experience
```

5. **Запусти игру через `iGP11.Launcher`** как обычно.

## Как проверить что всё работает

После запуска игры открой `proxychain.log` рядом с DLL. Все строки
должны быть с `OK`:

```
[1] LoadLibrary: ...\DS3_ProperPCExperience_Mod.dll
    -> OK
[H1] d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
     -> patched 1 IAT slot(s)
[*] LoadLibrary (original): ...\iGP11_Direct3D11_real.dll
    -> OK
```

В игре:

- **Proper PC Experience** работает, если интро FromSoftware/Bandai
  **проскипалось**, FPS не зажат на 60, при alt-tab нет долгого
  чёрного экрана.
- **iGP11** работает, если кастомные текстуры (сами иконки клавиатуры)
  на месте.

## По желанию: настройки Proper PC Experience

Мод читает три отдельных однострочных конфига **из рабочей директории
игры** (`...\Dark Souls III\`, где лежит `DarkSoulsIII.exe` — *не*
папка iGP11):

| Файл | Содержимое |
|---|---|
| `d3d11_refreshrate.ini` | refresh rate в Hz (например `120`) |
| `d3d11_fps.ini` | лимит FPS (например `144`) |
| `d3d11_fov.ini` | прибавка к FOV (например `0`) |

Можно вообще не создавать — мод сам выставит дефолты (макс. Hz моника,
120 FPS, +25° FOV на 16:9).

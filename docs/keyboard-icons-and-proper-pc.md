# Recipe: Keyboard Icons + DS3 Proper PC Experience

Run [Keyboard Icons](https://www.nexusmods.com/darksouls3/mods/294) and
[DS3 Proper PC Experience Mod](https://www.nexusmods.com/darksouls3/mods/1545)
at the same time in Dark Souls 3.

> *Russian version: [keyboard-icons-and-proper-pc.ru.md](keyboard-icons-and-proper-pc.ru.md).*

## Prerequisites

- DS3 already running through iGP11 (Keyboard Icons works out of the
  box that way). The iGP11 folder is wherever `iGP11.Direct3D11.dll`
  currently lives — usually `...\iGP11\`. Adjust paths below to match.
- `iGP11.Direct3D11.dll` from the latest
  [proxychain release](https://github.com/k1zn/igp11-proxychain/releases).
- `d3d11.dll` from
  [DS3 Proper PC Experience Mod](https://www.nexusmods.com/darksouls3/mods/1545).

## Steps

1. **Rename the original iGP11 DLL** in your iGP11 folder:

   ```
   iGP11.Direct3D11.dll  →  iGP11_Direct3D11_real.dll
   ```

2. **Drop in the proxy.** Copy `iGP11.Direct3D11.dll` from the
   proxychain release zip into the same iGP11 folder.

3. **Rename Proper PC Experience** from `d3d11.dll` to
   `DS3_ProperPCExperience_Mod.dll` and put it in the iGP11 folder
   (the rename avoids colliding with the system `d3d11.dll`).

4. **Create the config** `iGP11.Direct3D11.dll.ini` in the iGP11
   folder with this content:

   ```ini
   [Settings]
   LogEnabled = 1

   [DLLs]
   1 = DS3_ProperPCExperience_Mod.dll

   [IAT_Hooks]
   1 = d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
   ```

Final layout — these four files should sit next to each other:

```
...\iGP11\
├── iGP11.Direct3D11.dll              ← proxychain (this project)
├── iGP11.Direct3D11.dll.ini          ← the config above
├── iGP11_Direct3D11_real.dll         ← renamed original iGP11
└── DS3_ProperPCExperience_Mod.dll    ← renamed Proper PC Experience
```

5. **Launch the game via `iGP11.Launcher`** as usual.

## Verifying it worked

Open `proxychain.log` (next to the DLL) after the game starts. You
should see all `OK`'s:

```
[1] LoadLibrary: ...\DS3_ProperPCExperience_Mod.dll
    -> OK
[H1] d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
     -> patched 1 IAT slot(s)
[*] LoadLibrary (original): ...\iGP11_Direct3D11_real.dll
    -> OK
```

In-game:

- **Proper PC Experience** is active if the FromSoftware/Bandai
  intro is **skipped**, the FPS isn't capped at 60, alt-tabbing
  doesn't trigger a long black flash, etc.
- **iGP11** is active if your texture overrides (the keyboard icons
  themselves) still show.

## Optional: tweak Proper PC Experience

The mod reads three single-number config files **from the game's
working directory** (`...\Dark Souls III\`, where `DarkSoulsIII.exe`
lives — *not* the iGP11 folder):

| File | Content |
|---|---|
| `d3d11_refreshrate.ini` | refresh rate in Hz (e.g. `120`) |
| `d3d11_fps.ini` | FPS cap (e.g. `144`) |
| `d3d11_fov.ini` | FOV addition over default (e.g. `0`) |

Leave them out to use the mod's defaults (your monitor's max Hz, 120
FPS cap, +25° FOV on 16:9).

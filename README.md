# iGP11 proxychain

A drop-in replacement for **`iGP11.Direct3D11.dll`** that lets you chain
extra DLLs and IAT-hook the game's imports — *without breaking iGP11*.
Built specifically for the [iGP11](https://www.nexusmods.com/darksouls3/mods/394)
texture mod (Dark Souls 3 and similar titles).

> **Heads up:** this library is **unsupported** and was **vibe-coded with
> Claude Opus 4.7**. It works for the author's setup and is shared as-is.
> Issues / PRs are welcome but no guarantees of response, fixes, or
> compatibility with future iGP11 builds.
>
> *Russian version: see [README_ru.md](README_ru.md).*

## Why?

I wanted to run two Dark Souls 3 mods at the same time:

* [**Keyboard Icons**](https://www.nexusmods.com/darksouls3/mods/294) —
  ships as iGP11 texture overrides, so it requires
  `iGP11.Direct3D11.dll`.
* [**DS3 Proper PC Experience Mod**](https://www.nexusmods.com/darksouls3/mods/1545)
  — ships as `d3d11.dll`, so it needs the game to load it as a d3d11
  proxy and call its `D3D11CreateDevice` export (that's where it
  installs its DXGI hooks for refresh rate / FPS cap / FOV / skip
  intro).

The two refuse to coexist out of the box. iGP11 is loaded by its own
launcher via remote-thread injection — by then the game has long since
resolved `d3d11.dll` to the system DLL, so `DS3_ProperPCExperience_Mod`
dropped as `d3d11.dll` in the game folder ends up either ignored or
breaking iGP11, depending on load order. iGP11's launcher also doesn't
expose a way to chain other mods.

This project is the fix: a proxy `iGP11.Direct3D11.dll` that keeps
iGP11 working **and** loads Proper PC Experience (or any other passive
d3d11-style mod) alongside it, rewriting the game EXE's IAT so the
mod's `D3D11CreateDevice` actually gets called.

## What it does

iGP11 is loaded into the game by `iGP11.Tool.exe` via remote-thread
injection. The injector calls `iGP11.Direct3D11.dll` exports
`get` / `start` / `stop` to install its texture hooks.

This project ships a **proxy DLL** named exactly `iGP11.Direct3D11.dll`
which the injector loads instead of the real one. On `DllMain` the proxy:

1. Reads `iGP11.Direct3D11.dll.ini` next to itself.
2. `LoadLibrary`s every DLL listed in `[DLLs]` (mods, debug helpers,
   ReShade addons — whatever you want).
3. Walks the game EXE's import table and rewrites the IAT entries listed
   in `[IAT_Hooks]`, so passive d3d11-style mods (e.g.
   *DS3 ProperPCExperience*) actually receive their `D3D11CreateDevice`
   call even though they aren't named `d3d11.dll`.
4. Loads the renamed real iGP11 DLL (`iGP11_Direct3D11_real.dll`).
5. Forwards `get` / `start` / `stop` to it, so iGP11 keeps working
   exactly as before.

End result: iGP11 keeps doing textures **and** any d3d11.dll-style mod
gets to install its own hooks at the same time.

```
              ┌────────────────────────────────┐
              │ DarkSoulsIII.exe (host)        │
              │   imports d3d11.dll!D3D11Create│ ◄── IAT rewritten ◄─┐
              └────────────────┬───────────────┘                     │
                  iGP11.Tool injects iGP11.dll                       │
                               │ LoadLibrary                         │
                               ▼                                     │
              ┌────────────────────────────────┐                     │
              │ iGP11.Direct3D11.dll (us)      │                     │
              │   - reads .ini                 │                     │
              │   - LoadLibrary mod DLLs ──────┼──► your_mod.dll ────┘
              │   - patches game IAT           │
              │   - forwards get/start/stop ──►│  ┌──────────────────────────────┐
              └────────────────────────────────┘  │ iGP11_Direct3D11_real.dll     │
                                                   │ (renamed original iGP11)      │
                                                   └──────────────────────────────┘
```

## Quick start (no compiler required)

You need the prebuilt DLL — grab the latest from
[Releases](https://github.com/k1zn/igp11-proxychain/releases),
or build it yourself (see [Build](#build) below).

### 1. Set up files in your iGP11 folder

The folder where `iGP11.Direct3D11.dll` already lives (e.g.
`D:\Games\DarkSoulsMods\iGP11\`):

| Action | File |
|---|---|
| Rename the existing | `iGP11.Direct3D11.dll` → `iGP11_Direct3D11_real.dll` (underscores!) |
| Copy in the proxy | `iGP11.Direct3D11.dll` (from this project) |
| Copy + rename | `iGP11.Direct3D11.dll.ini.example` → `iGP11.Direct3D11.dll.ini` |

### 2. Edit `iGP11.Direct3D11.dll.ini`

Bare minimum example (loading one extra mod):

```ini
[Settings]
LogEnabled = 1

[DLLs]
1 = DS3_ProperPCExperience_Mod.dll

[IAT_Hooks]
1 = d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
```

> **Note on this example:** Proper PC Experience Mod ships as `d3d11.dll`.
> For this config to work, **rename it to `DS3_ProperPCExperience_Mod.dll`**
> and drop it next to the proxy DLL (same folder as
> `iGP11.Direct3D11.dll`). The rename is just so the filename doesn't
> collide with the system `d3d11.dll`; the IAT hook tells the game's
> `d3d11.dll!D3D11CreateDevice` to route through the renamed mod
> instead.

Paths in `[DLLs]` are absolute, or relative to the proxy DLL.

### 3. Launch the game

Launch via `iGP11.Launcher` as you normally would. A `proxychain.log`
file will appear next to the DLL showing what happened. Look for:

```
[1] LoadLibrary: ...\DS3_ProperPCExperience_Mod.dll
    -> OK, base = 0x...
[H1] d3d11.dll!D3D11CreateDevice -> DS3_ProperPCExperience_Mod.dll!D3D11CreateDevice
     -> patched 1 IAT slot(s) -> 0x...
[*] LoadLibrary (original): ...\iGP11_Direct3D11_real.dll
    -> OK, base = 0x...
```

If everything shows `OK`, you're done.

## Configuration reference

### `[Settings]`

| Key | Default | Description |
|---|---|---|
| `LogEnabled` | `1` | `0` to disable logging entirely. |
| `LogFile` | `proxychain.log` | Path (relative to proxy DLL or absolute). |
| `LoadOriginal` | `1` | Explicitly `LoadLibrary` the renamed original. `0` lets Windows resolve it lazily via forwarders. |

### `[DLLs]`

Key=value pairs, loaded in file order. Keys are arbitrary (must be unique
inside the section); values are DLL paths (absolute or relative to the
proxy DLL).

### `[IAT_Hooks]`

Each line redirects one host EXE import to a function inside one of the
DLLs you loaded above.

```
<num> = target_dll!target_func -> resolver_dll!resolver_func
```

- `target_dll` — the import as it appears in the host EXE (e.g.
  `d3d11.dll`, case-insensitive).
- `target_func` — the imported function name (case-sensitive).
- `resolver_dll` — full filename of a DLL already loaded via `[DLLs]`.
- `resolver_func` — the export to redirect calls to.

The log prints `patched N IAT slot(s)` if the rewrite succeeded, or
`0 IAT slots matched` if the host doesn't statically import that
function.

### Why is `[IAT_Hooks]` needed?

Some d3d11-style mods (refresh-rate fixers, ReShade, ENB, etc.) install
their hooks **inside their exported `D3D11CreateDevice`**, not in
`DllMain`. If you just `LoadLibrary` them, nothing happens — the export
must be **called** by the game.

`[IAT_Hooks]` rewrites the import table so the game's call to
`d3d11.dll!D3D11CreateDevice` lands in the mod's `D3D11CreateDevice`
instead. The mod then installs its hooks and forwards the call into the
real `d3d11.dll`. Both iGP11 *and* the mod end up active.

## Build

You need:

- **MinGW-w64**, x86_64. Easiest: portable
  [w64devkit](https://github.com/skeeto/w64devkit/releases) — one ZIP, no
  installer.
- **Python 3** (any recent version).

Steps:

```sh
# Open w64devkit.exe, cd into the project root, then:

# 1. Generate the forwarder .def file from YOUR copy of iGP11.Direct3D11.dll.
#    The committed src/proxy.def is what we extracted from upstream iGP11;
#    regenerate if you have a different build.
python tools/gen_def.py /path/to/iGP11.Direct3D11.dll iGP11_Direct3D11_real src/proxy.def

# 2. Build.
make
```

Output: `iGP11.Direct3D11.dll` in the project root. Drop it into your
iGP11 folder as described in [Quick start](#quick-start-no-compiler-required).

### Custom renamed-original name

If you want a different name for the renamed original (default is
`iGP11_Direct3D11_real.dll`), regenerate the `.def` with the new target
and pass it to `make`:

```sh
python tools/gen_def.py /path/to/iGP11.Direct3D11.dll iGP11_orig src/proxy.def
make REAL_DLL=iGP11_orig.dll
```

The target name **must not contain dots** beyond the `.dll` extension —
Windows' loader splits forwarder strings on the first dot, so
`my.original.dll` would never resolve at runtime.

## Project layout

```
.
├── README.md              # this file
├── README_ru.md           # Russian version
├── CLAUDE.md              # notes for AI assistants working on the codebase
├── LICENSE                # MIT
├── Makefile               # builds via MinGW-w64
├── iGP11.Direct3D11.dll.ini.example
├── src/
│   ├── dllmain.cpp        # DllMain, INI loop, IAT-hook loop
│   ├── config.{h,cpp}     # INI parser (GetPrivateProfileSectionW)
│   ├── log.{h,cpp}        # UTF-8 file logger (CreateFile/WriteFile)
│   ├── iat_patch.{h,cpp}  # Walks PE imports, rewrites IAT entries
│   └── proxy.def          # Forwarder definitions (generated)
├── tools/
│   └── gen_def.py         # Zero-deps PE parser → .def generator
└── .github/workflows/
    └── build.yml          # CI: builds the DLL on every push
```

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Game crashes immediately, log says `LoadLibrary (original): -> FAILED` | The renamed original isn't where the proxy expects it, or has the wrong name. Default is `iGP11_Direct3D11_real.dll` (underscores). |
| `0 IAT slots matched` for a hook | The host EXE doesn't statically import that function from that DLL. Double-check the import name with `dumpbin /imports` or `objdump -p`. |
| `proxychain.log` not appearing | `LogEnabled = 0` in the INI, or no write permission in the iGP11 folder. |
| The mod loads but its features don't work | The mod probably installs its hooks inside an exported function — add a `[IAT_Hooks]` line to redirect the game's call into that export. |
| Build error `ERROR: src/proxy.def not found` | Run `python tools/gen_def.py ... src/proxy.def` first. |

## License

MIT — see [LICENSE](LICENSE). This project contains no code or assets
from iGP11 or any third-party mod; it only references their public
export names by string.

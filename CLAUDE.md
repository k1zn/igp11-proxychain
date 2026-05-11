# CLAUDE.md — guidance for AI assistants

This is a small Windows x86_64 DLL that acts as a proxy for
`iGP11.Direct3D11.dll`. It loads extra mods from an INI file and
patches the host EXE's IAT, then forwards iGP11's three exports
(`get` / `start` / `stop`) to a renamed copy of the original.

## Quick orientation

- `src/dllmain.cpp` — `DllMain` only. On `DLL_PROCESS_ATTACH`:
  parses the INI, `LoadLibrary`s each entry in `[DLLs]`, applies
  `[IAT_Hooks]` against the host EXE, then loads the renamed original.
- `src/config.{h,cpp}` — INI parser built on
  `GetPrivateProfileSectionW`. Adds new sections by extending the
  `read_section_values` calls inside `Config::load`.
- `src/iat_patch.{h,cpp}` — walks the host module's import table and
  rewrites a single IAT slot. Self-contained, no dependencies beyond
  `<windows.h>`.
- `src/log.{h,cpp}` — UTF-8 file logger backed directly by
  `CreateFile` / `WriteFile` to avoid CRT printf locale issues.
- `src/proxy.def` — forwarder definitions (regenerate with
  `tools/gen_def.py`). Forward target name must not contain dots.
- `tools/gen_def.py` — zero-deps PE-export extractor.

## How to build and verify locally

```sh
make            # produces iGP11.Direct3D11.dll in the project root
make clean      # removes build/ and the output DLL
```

There's no formal test suite. Smoke-test the build by loading the DLL
via Python+ctypes from a copy of an iGP11 install folder:

```python
import ctypes, os
os.chdir(r"D:\Games\DarkSoulsMods\iGP11")
ctypes.WinDLL("./iGP11.Direct3D11.dll")
```

Then check that `proxychain.log` looks right — sections in the log
correspond to `[DLLs]`, `[IAT_Hooks]`, and the final original load.

## Constraints to remember

- **x86_64 only.** iGP11 and Dark Souls 3 are 64-bit. Don't add
  conditional 32-bit support without checking the rest of the toolchain.
- **No dots in the renamed-original filename.** Windows' loader splits
  forwarder strings on the **first** dot, so a target like
  `iGP11.Direct3D11.real.dll` makes every export resolve to
  `iGP11.Direct3D11.dll!real.get`, which doesn't exist. The default
  `iGP11_Direct3D11_real.dll` (underscores) is correct. `gen_def.py`
  refuses dotted target names; keep that guard.
- **Do not call CRT-locked APIs during `DllMain`.** Logging via
  `CreateFile`/`WriteFile` is fine; `_wfopen` mode strings differ
  between MSVC and MinGW.
- **Match `PROXYCHAIN_REAL_DLL_W` and the forwarder target.** The
  `#define` in `dllmain.cpp` and the `.def` forwards must point at the
  same renamed-DLL name; the Makefile keeps them in sync via
  `REAL_DLL`.
- **Don't commit a freshly built `iGP11.Direct3D11.dll`.** It's a
  build artifact; the GitHub workflow produces it on demand.

## Common tasks

### Adding a new INI section

1. Add a struct field to `Config` in `src/config.h`.
2. Read it in `Config::load` (use the `read_section_values` lambda
   already in there for line-per-entry sections, or
   `GetPrivateProfileStringW` for `[Settings]`-style key/value).
3. Consume it in `src/dllmain.cpp` after the existing `[DLLs]` /
   `[IAT_Hooks]` blocks. Log each entry as it's processed.

### Adding a new hook type

Add a new function in `src/iat_patch.cpp` (e.g. an export-table
rewriter for hooking outbound calls), expose it in
`src/iat_patch.h`, then call it from `on_attach` in `dllmain.cpp`.
Try to keep `iat_patch.cpp` free of STL types so it stays self-contained.

### Changing build flags

`Makefile` only — keep changes there, don't add a CMakeLists. The
flags aim for a compact DLL: `-Os`, `-fno-exceptions`, `-fno-rtti`,
`-fvisibility=hidden`, `-Wl,--gc-sections`. If you remove
`-static-libstdc++`, ensure the resulting DLL still loads without
`libstdc++-6.dll` on a clean Windows.

## Style

- Plain C++17. Stick to `<string>`, `<vector>`, raw Win32; no Boost,
  no third-party headers.
- Existing code uses 4-space indents, snake_case for functions and
  variables, CamelCase for structs. Match the surrounding style.
- Logging strings stay in English; the user-facing READMEs hold
  Russian text and are not consumed by the runtime.
- Comments explain *why*, not *what*; the code itself is short enough
  to read top-to-bottom.

## Things outside the project's scope

- VTable hooking / inline trampolines (use a real lib like MinHook
  if a future feature actually needs them — out of scope for
  proxychain).
- Anti-debug / anti-detection. This is a mod-loader for a single-player
  game; keep it simple.
- Other games. iGP11 is the supported target; helping other proxy
  setups is a non-goal unless trivially compatible.

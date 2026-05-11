#pragma once

#include <windows.h>

// Walk the import table of `host` and rewrite every IAT entry that imports
// `target_func` from `target_dll` to point at `new_func`.
//
// `target_dll`  : import DLL name as it appears in the host's import table,
//                 case-insensitive (e.g. "d3d11.dll").
// `target_func` : exported name (case-sensitive); ordinal-only imports are
//                 skipped.
// `host`        : module whose imports to patch. Pass NULL to use the main
//                 process EXE (GetModuleHandle(NULL)).
//
// Returns the number of IAT slots actually rewritten (0 if not found, can be
// >1 if the host imports the same function from the same DLL more than once).
int iat_patch(HMODULE host, const char* target_dll, const char* target_func, void* new_func);

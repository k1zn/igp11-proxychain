#include "iat_patch.h"

#include <string.h>

static int strcasecmp_ascii(const char* a, const char* b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (int)ca - (int)cb;
        ++a; ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int iat_patch(HMODULE host, const char* target_dll, const char* target_func, void* new_func) {
    if (!host) host = GetModuleHandleW(nullptr);
    if (!host || !target_dll || !target_func || !new_func) return 0;

    auto base = reinterpret_cast<BYTE*>(host);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    DWORD imp_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (imp_rva == 0) return 0;

    auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imp_rva);
    int patched = 0;

    for (; desc->Name; ++desc) {
        const char* dll = reinterpret_cast<const char*>(base + desc->Name);
        if (strcasecmp_ascii(dll, target_dll) != 0) continue;

        // OriginalFirstThunk (INT) preserves names even after the loader
        // bound the IAT. Fall back to FirstThunk for old binaries that have
        // no INT, but in that case the IAT still holds the original RVAs
        // until binding.
        DWORD int_rva  = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        DWORD iat_rva  = desc->FirstThunk;

        auto int_thunks = reinterpret_cast<IMAGE_THUNK_DATA*>(base + int_rva);
        auto iat_thunks = reinterpret_cast<IMAGE_THUNK_DATA*>(base + iat_rva);

        for (; int_thunks->u1.AddressOfData; ++int_thunks, ++iat_thunks) {
            if (IMAGE_SNAP_BY_ORDINAL(int_thunks->u1.Ordinal)) continue;
            auto by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + int_thunks->u1.AddressOfData);
            if (strcmp(by_name->Name, target_func) != 0) continue;

            DWORD old_protect = 0;
            if (!VirtualProtect(&iat_thunks->u1.Function, sizeof(void*),
                                PAGE_READWRITE, &old_protect)) {
                continue;
            }
            iat_thunks->u1.Function = reinterpret_cast<ULONGLONG>(new_func);
            VirtualProtect(&iat_thunks->u1.Function, sizeof(void*),
                           old_protect, &old_protect);
            ++patched;
        }
    }

    return patched;
}

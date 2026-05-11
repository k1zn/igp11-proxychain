// proxychain DllMain - loads DLLs from an INI config in DLL_PROCESS_ATTACH,
// then transparently forwards all exports (e.g. of iGP11.Direct3D11.dll)
// to a renamed original DLL via the export forwarders defined in
// src/proxy.def.

#include <windows.h>

#include <string>
#include <vector>

#include "config.h"
#include "iat_patch.h"
#include "log.h"

// Name of the renamed original DLL. MUST match the forwarder target used
// when generating src/proxy.def (without the .dll extension).
//
// Build-time override: pass -DPROXYCHAIN_REAL_DLL_W=L\"my_real.dll\".
//
// Note: dots inside the filename break Windows' forwarder resolution
// (the loader splits "a.b.c.func" on the first dot), so this name uses
// underscores instead.
#ifndef PROXYCHAIN_REAL_DLL_W
#define PROXYCHAIN_REAL_DLL_W L"iGP11_Direct3D11_real.dll"
#endif

static HMODULE g_self = nullptr;
static std::vector<HMODULE> g_loaded;
static HMODULE g_real = nullptr;

static std::wstring get_module_path(HMODULE m) {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameW(m, buf.data(), (DWORD)buf.size());
        if (n == 0) return L"";
        if (n < buf.size()) return std::wstring(buf.data(), n);
        buf.resize(buf.size() * 2);
        if (buf.size() > 65536) return L"";
    }
}

static std::wstring dirname(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

static std::wstring filename(const std::wstring& path) {
    auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

static void load_one(const std::wstring& dir, const std::wstring& spec, size_t idx) {
    std::wstring full = is_absolute(spec) ? spec : (dir + L"\\" + spec);
    log_writef("[%zu] LoadLibrary: %s", idx, utf8(full).c_str());
    HMODULE h = LoadLibraryW(full.c_str());
    if (!h) {
        DWORD e = GetLastError();
        log_writef("    -> FAILED, GetLastError = %lu (0x%08lX)", e, e);
    } else {
        log_writef("    -> OK, base = 0x%p", (void*)h);
        g_loaded.push_back(h);
    }
}

static void on_attach(HMODULE self) {
    g_self = self;

    std::wstring self_path = get_module_path(self);
    std::wstring dir = dirname(self_path);
    std::wstring name = filename(self_path);
    std::wstring ini_path = dir + L"\\" + name + L".ini";

    Config cfg;
    cfg.load(ini_path);

    if (cfg.log_enabled) {
        std::wstring log_path = is_absolute(cfg.log_file)
            ? cfg.log_file
            : (dir + L"\\" + cfg.log_file);
        log_open(log_path.c_str());
    }

    log_writef("================ proxychain attach ================");
    log_writef("Self : %s", utf8(self_path).c_str());
    log_writef("Ini  : %s", utf8(ini_path).c_str());
    log_writef("Real : %s", utf8(PROXYCHAIN_REAL_DLL_W).c_str());
    log_writef("Configured DLLs: %zu", cfg.dlls.size());

    // 1. User DLLs in declared order.
    for (size_t i = 0; i < cfg.dlls.size(); ++i) {
        load_one(dir, cfg.dlls[i], i + 1);
    }

    // 2. IAT hooks against the host EXE. We do this *before* loading the
    //    original so that any side-effects of the renamed-original's
    //    DllMain happen with the patched IAT already in place.
    if (!cfg.iat_hooks.empty()) {
        HMODULE host = GetModuleHandleW(nullptr);
        log_writef("Applying %zu IAT hook(s) to host module 0x%p",
                   cfg.iat_hooks.size(), (void*)host);

        for (size_t i = 0; i < cfg.iat_hooks.size(); ++i) {
            const IatHook& h = cfg.iat_hooks[i];
            log_writef("[H%zu] %s!%s -> %ls!%s",
                       i + 1,
                       h.target_dll.c_str(), h.target_func.c_str(),
                       h.resolver_dll.c_str(), h.resolver_func.c_str());

            HMODULE r = GetModuleHandleW(h.resolver_dll.c_str());
            if (!r) {
                log_writef("     -> resolver DLL not loaded; "
                           "make sure it's listed in [DLLs] above");
                continue;
            }
            FARPROC fn = GetProcAddress(r, h.resolver_func.c_str());
            if (!fn) {
                log_writef("     -> GetProcAddress failed (GLE=%lu)",
                           GetLastError());
                continue;
            }
            int n = iat_patch(host, h.target_dll.c_str(),
                              h.target_func.c_str(), (void*)fn);
            if (n == 0) {
                log_writef("     -> 0 IAT slots matched; the host might not "
                           "import this function statically");
            } else {
                log_writef("     -> patched %d IAT slot(s) -> 0x%p",
                           n, (void*)fn);
            }
        }
    }

    // 3. The renamed original last (so its DllMain runs after the user mods).
    //    The OS will resolve forwarders to it lazily anyway, but loading it
    //    explicitly here gives us a deterministic order and a clear log entry.
    if (cfg.load_original) {
        std::wstring real_path = dir + L"\\" + PROXYCHAIN_REAL_DLL_W;
        log_writef("[*] LoadLibrary (original): %s", utf8(real_path).c_str());
        g_real = LoadLibraryW(real_path.c_str());
        if (!g_real) {
            DWORD e = GetLastError();
            log_writef("    -> FAILED, GetLastError = %lu (0x%08lX)", e, e);
            log_writef("    !! WITHOUT THE ORIGINAL DLL THE GAME WILL CRASH !!");
        } else {
            log_writef("    -> OK, base = 0x%p", (void*)g_real);
        }
    }

    log_writef("================ proxychain ready =================");
}

static void on_detach() {
    log_writef("================ proxychain detach ================");
    log_close();
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        on_attach((HMODULE)hModule);
        break;
    case DLL_PROCESS_DETACH:
        on_detach();
        break;
    }
    return TRUE;
}

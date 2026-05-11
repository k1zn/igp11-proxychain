#include "config.h"

#include <windows.h>
#include <vector>

bool is_absolute(const std::wstring& p) {
    if (p.size() >= 2 && p[1] == L':') return true;
    if (!p.empty() && (p[0] == L'\\' || p[0] == L'/')) return true;
    return false;
}

static std::wstring rtrim(std::wstring s) {
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t' ||
                          s.back() == L'\r' || s.back() == L'\n'))
        s.pop_back();
    return s;
}

static std::wstring ltrim(std::wstring s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    return s.substr(i);
}

static std::wstring trim(std::wstring s) { return rtrim(ltrim(std::move(s))); }

static std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), (int)s.size(), out.data(), n,
                        nullptr, nullptr);
    return out;
}

// Parse a single line of the form
//   target_dll!target_func -> resolver_dll!resolver_func
// Returns false if it doesn't match the expected shape.
static bool parse_hook_line(const std::wstring& value, IatHook& out) {
    auto arrow = value.find(L"->");
    if (arrow == std::wstring::npos) return false;
    std::wstring lhs = trim(value.substr(0, arrow));
    std::wstring rhs = trim(value.substr(arrow + 2));

    auto bang_l = lhs.find(L'!');
    auto bang_r = rhs.find(L'!');
    if (bang_l == std::wstring::npos || bang_r == std::wstring::npos) return false;

    std::wstring tdll  = trim(lhs.substr(0, bang_l));
    std::wstring tfunc = trim(lhs.substr(bang_l + 1));
    std::wstring rdll  = trim(rhs.substr(0, bang_r));
    std::wstring rfunc = trim(rhs.substr(bang_r + 1));

    if (tdll.empty() || tfunc.empty() || rdll.empty() || rfunc.empty()) return false;

    out.target_dll   = narrow(tdll);
    out.target_func  = narrow(tfunc);
    out.resolver_dll = rdll;
    out.resolver_func = narrow(rfunc);
    return true;
}

void Config::load(const std::wstring& ini_path) {
    // [Settings]
    log_enabled = GetPrivateProfileIntW(L"Settings", L"LogEnabled", 1,
                                        ini_path.c_str()) != 0;
    load_original = GetPrivateProfileIntW(L"Settings", L"LoadOriginal", 1,
                                          ini_path.c_str()) != 0;

    wchar_t buf[MAX_PATH];
    GetPrivateProfileStringW(L"Settings", L"LogFile", L"proxychain.log", buf,
                             MAX_PATH, ini_path.c_str());
    log_file = buf;

    std::vector<wchar_t> section(64 * 1024);

    auto read_section_values = [&](const wchar_t* name, auto consume) {
        DWORD got = GetPrivateProfileSectionW(name, section.data(),
                                              (DWORD)section.size(),
                                              ini_path.c_str());
        if (got == 0) return;
        wchar_t* p = section.data();
        while (*p) {
            std::wstring entry = p;
            size_t adv = entry.size() + 1;
            std::wstring trimmed = trim(entry);
            if (!trimmed.empty() && trimmed[0] != L';' && trimmed[0] != L'#') {
                auto pos = entry.find(L'=');
                if (pos != std::wstring::npos) {
                    std::wstring value = trim(entry.substr(pos + 1));
                    if (!value.empty()) consume(value);
                }
            }
            p += adv;
        }
    };

    // [DLLs] - DLLs to LoadLibrary in file order, before the original.
    read_section_values(L"DLLs", [&](const std::wstring& value) {
        dlls.push_back(value);
    });

    // [IAT_Hooks] - IAT redirects to apply to the host EXE after the
    // resolver DLLs are loaded. Lines that don't parse are silently
    // skipped here; dllmain logs them.
    read_section_values(L"IAT_Hooks", [&](const std::wstring& value) {
        IatHook h;
        if (parse_hook_line(value, h)) iat_hooks.push_back(std::move(h));
    });
}

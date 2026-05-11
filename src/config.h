#pragma once

#include <string>
#include <vector>

struct IatHook {
    std::string target_dll;       // e.g. "d3d11.dll"
    std::string target_func;      // e.g. "D3D11CreateDevice"
    std::wstring resolver_dll;    // e.g. L"DS3_ProperPCExperience_Mod.dll"
    std::string resolver_func;    // e.g. "D3D11CreateDevice"
};

struct Config {
    bool log_enabled = true;
    std::wstring log_file = L"proxychain.log";
    bool load_original = true;
    std::vector<std::wstring> dlls;
    std::vector<IatHook> iat_hooks;

    void load(const std::wstring& ini_path);
};

bool is_absolute(const std::wstring& p);

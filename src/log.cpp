#include "log.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

static HANDLE g_h = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_cs;
static bool g_cs_init = false;

std::string utf8(const wchar_t* s) {
    if (!s || !*s) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), n, nullptr, nullptr);
    return out;
}

void log_open(const wchar_t* path) {
    if (!g_cs_init) {
        InitializeCriticalSection(&g_cs);
        g_cs_init = true;
    }
    EnterCriticalSection(&g_cs);
    if (g_h != INVALID_HANDLE_VALUE) {
        CloseHandle(g_h);
        g_h = INVALID_HANDLE_VALUE;
    }
    // CREATE_ALWAYS truncates the previous log on every load - only the
    // current session is kept, which is what people actually want when
    // they look at proxychain.log.
    g_h = CreateFileW(path,
                      GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr,
                      CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL,
                      nullptr);
    if (g_h != INVALID_HANDLE_VALUE) {
        const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
        DWORD written = 0;
        WriteFile(g_h, bom, 3, &written, nullptr);
    }
    LeaveCriticalSection(&g_cs);
}

void log_close() {
    if (!g_cs_init) return;
    EnterCriticalSection(&g_cs);
    if (g_h != INVALID_HANDLE_VALUE) {
        CloseHandle(g_h);
        g_h = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&g_cs);
}

void log_writef(const char* fmt, ...) {
    if (!g_cs_init) return;
    EnterCriticalSection(&g_cs);
    if (g_h != INVALID_HANDLE_VALUE) {
        char prefix[64];
        SYSTEMTIME st;
        GetLocalTime(&st);
        int pn = snprintf(prefix, sizeof(prefix),
                          "[%02d:%02d:%02d.%03d] ",
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        if (pn < 0) pn = 0;
        if (pn > (int)sizeof(prefix) - 1) pn = sizeof(prefix) - 1;

        char body[2048];
        va_list args;
        va_start(args, fmt);
        int bn = vsnprintf(body, sizeof(body), fmt, args);
        va_end(args);
        if (bn < 0) bn = 0;
        if (bn > (int)sizeof(body) - 1) bn = sizeof(body) - 1;

        DWORD written = 0;
        WriteFile(g_h, prefix, (DWORD)pn, &written, nullptr);
        WriteFile(g_h, body, (DWORD)bn, &written, nullptr);
        WriteFile(g_h, "\r\n", 2, &written, nullptr);
        FlushFileBuffers(g_h);
    }
    LeaveCriticalSection(&g_cs);
}

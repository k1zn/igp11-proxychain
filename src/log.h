#pragma once

#include <string>

void log_open(const wchar_t* path);
void log_close();
void log_writef(const char* fmt, ...);

std::string utf8(const wchar_t* s);
inline std::string utf8(const std::wstring& s) { return utf8(s.c_str()); }

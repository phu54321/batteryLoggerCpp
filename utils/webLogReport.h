#pragma once

#include <windows.h>
#include <string>

std::wstring createWebLogReport(HINSTANCE hInstance, const std::wstring &logPath);

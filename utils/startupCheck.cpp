// Copyright (C) 2025 Park Hyunwoo
//
// This file is part of batteryLoggerCpp.
//
// batteryLoggerCpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// batteryLoggerCpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with batteryLoggerCpp.  If not, see <https://www.gnu.org/licenses/>.

#define _CRT_SECURE_NO_WARNINGS

#include "startupCheck.h"

#include <windows.h>
#include <shellapi.h>
#include <cwctype>
#include <cstdlib>
#include <string>

static std::wstring getStartupDir() {
    const wchar_t *appData = _wgetenv(L"APPDATA");
    if (!appData || !*appData) {
        return {};
    }

    return std::wstring(appData) + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
}

static std::wstring getCurrentExePath() {
    wchar_t path[MAX_PATH + 1] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }

    return path;
}

static std::wstring lowercaseString(std::wstring text) {
    for (wchar_t &ch: text) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return text;
}

static std::wstring getFullPath(const std::wstring &path) {
    wchar_t fullPath[MAX_PATH + 1] = {};
    DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr);
    if (length == 0 || length > MAX_PATH) {
        return {};
    }

    return fullPath;
}

static bool startsWith(const std::wstring &text, const std::wstring &prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

static bool isPathInsideDir(const std::wstring &path, const std::wstring &dir) {
    std::wstring normalizedPath = lowercaseString(getFullPath(path));
    std::wstring normalizedDir = lowercaseString(getFullPath(dir));
    if (normalizedPath.empty() || normalizedDir.empty()) {
        return false;
    }

    if (normalizedDir.back() != L'\\' && normalizedDir.back() != L'/') {
        normalizedDir += L"\\";
    }

    return startsWith(normalizedPath, normalizedDir);
}

static std::wstring getFilename(const std::wstring &path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return path;
    }

    return path.substr(pos + 1);
}

bool copyToStartupAndRestartIfNeeded() {
    std::wstring exePath = getCurrentExePath();
    std::wstring startupDir = getStartupDir();
    if (exePath.empty() || startupDir.empty() || isPathInsideDir(exePath, startupDir)) {
        return false;
    }

    int answer = MessageBoxW(
        nullptr,
        L"batteryLoggerCpp is not running from your Startup folder.\n\n"
        L"Copy it there and restart from the Startup folder?",
        L"batteryLoggerCpp",
        MB_YESNO | MB_ICONQUESTION
    );
    if (answer != IDYES) {
        return false;
    }

    if (!CreateDirectoryW(startupDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Could not create the Startup folder.", L"batteryLoggerCpp", MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring startupExePath = startupDir + L"\\" + getFilename(exePath);
    if (!CopyFileW(exePath.c_str(), startupExePath.c_str(), FALSE)) {
        MessageBoxW(nullptr, L"Could not copy batteryLoggerCpp to the Startup folder.", L"batteryLoggerCpp",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", startupExePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR) result <= 32) {
        MessageBoxW(nullptr, L"Could not start batteryLoggerCpp from the Startup folder.", L"batteryLoggerCpp",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

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

#include "batteryLog.h"
#include "batteryStatus.h"

#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

static std::string getCurrentIsoTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    std::stringstream ss;
    ss << std::put_time(&tmLocal, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Expand "~" to user home directory (rough equivalent of os.path.expanduser("~"))
static std::wstring getHomeDir() {
    const wchar_t *userProfile = _wgetenv(L"USERPROFILE");
    if (userProfile && *userProfile) {
        return userProfile;
    }
    // Fallback to current directory
    return L".";
}

std::wstring getLogPath() {
    std::wstring home = getHomeDir();
    std::wstring path = home + L"\\batteryLog.csv";
    return path;
}

void logTask(const std::string &machineId) {
    bool plugged = false;
    int percent = -1;
    if (!getBatteryStatus(plugged, percent)) {
        // If we can't get battery status, just return; you can also log an error if you want.
        return;
    }

    auto logPath = getLogPath();
    bool fileExists = GetFileAttributesW(logPath.c_str()) != INVALID_FILE_ATTRIBUTES;

    auto fp = _wfopen(logPath.c_str(), L"a+");
    if (!fp) {
        return;
    }

    fseek(fp, 0, SEEK_END);

    if (!fileExists || ftell(fp) == 0) {
        fprintf(fp, "time,plugged,percent,machine_id\n");
    }

    fprintf(fp, "%s,%s,%s,%s\n",
            getCurrentIsoTime().c_str(),
            plugged ? "True" : "False",
            std::to_string(percent).c_str(),
            machineId.c_str()
    );

    fclose(fp);
}

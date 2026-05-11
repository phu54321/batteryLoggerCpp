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

#include "singleInstance.h"

#include <windows.h>

// Single-instance guard using a named mutex (similar to tendo.singleton)
bool ensureSingleInstance() {
    // Change the name to something unique to your app if you want.
    HANDLE hMutex = CreateMutexW(
        nullptr,
        FALSE,
        L"Global\\BatteryLoggerSingleInstanceMutex"
    );

    if (!hMutex) {
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance already running.
        CloseHandle(hMutex);
        return false;
    }

    // Keep hMutex alive for the duration of the process.
    return true;
}

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

#include "batteryStatus.h"

#include <windows.h>

// Get battery status via WinAPI
bool getBatteryStatus(bool &plugged, int &percent) {
    SYSTEM_POWER_STATUS sps{};
    if (!GetSystemPowerStatus(&sps)) {
        return false;
    }

    plugged = (sps.ACLineStatus == 1);
    if (sps.BatteryLifePercent == 255) {
        // unknown
        percent = -1;
    } else {
        percent = static_cast<int>(sps.BatteryLifePercent);
    }
    return true;
}

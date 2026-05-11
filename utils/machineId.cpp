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

#include "machineId.h"

#include <windows.h>
#include <iphlpapi.h>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

// Simple 32-bit FNV-1a hash for MAC bytes (non-cryptographic but enough
// to avoid leaking the raw address directly).
static uint32_t fnv1a32(const unsigned char *data, size_t len) {
    const uint32_t FNV_OFFSET_BASIS = 2166136261u;
    const uint32_t FNV_PRIME = 16777619u;

    uint32_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint32_t>(data[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

// Get a stable machine id derived from the first non-zero MAC address,
// hashed to 32 bits and truncated to 4 hex characters (similar idea to Python).
std::string getMachineId() {
    ULONG bufLen = 0;
    if (GetAdaptersInfo(nullptr, &bufLen) != ERROR_BUFFER_OVERFLOW) {
        // Fallback if we can't query adapters: just hash some constant.
        uint32_t h = fnv1a32(reinterpret_cast<const unsigned char *>("fallback"), 8);
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (h & 0xFFFFu);
        return ss.str();
    }

    std::unique_ptr<BYTE[]> buffer(new BYTE[bufLen]);
    auto *pAdapterInfo = reinterpret_cast<IP_ADAPTER_INFO *>(buffer.get());

    if (GetAdaptersInfo(pAdapterInfo, &bufLen) != NO_ERROR) {
        uint32_t h = fnv1a32(reinterpret_cast<const unsigned char *>("fallback2"), 9);
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (h & 0xFFFFu);
        return ss.str();
    }

    unsigned char mac[6] = {0};
    bool found = false;
    for (IP_ADAPTER_INFO *p = pAdapterInfo; p != nullptr; p = p->Next) {
        if (p->AddressLength == 6) {
            bool nonzero = false;
            for (UINT i = 0; i < p->AddressLength; ++i) {
                if (p->Address[i] != 0) {
                    nonzero = true;
                    break;
                }
            }
            if (nonzero) {
                memcpy(mac, p->Address, 6);
                found = true;
                break;
            }
        }
    }

    if (!found) {
        uint32_t h = fnv1a32(reinterpret_cast<const unsigned char *>("fallback3"), 9);
        std::stringstream ss;
        ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (h & 0xFFFFu);
        return ss.str();
    }

    uint32_t hash = fnv1a32(mac, 6);
    std::stringstream ss;
    ss << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << (hash & 0xFFFFu);
    return ss.str();
}

// battery_logger.cpp
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <memory>

// Simple 32-bit FNV-1a hash for MAC bytes (non-cryptographic but enough
// to avoid leaking the raw address directly).
uint32_t fnv1a32(const unsigned char *data, size_t len) {
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

// Get ISO-8601-like local time string (YYYY-MM-DDTHH:MM:SS)
std::string getCurrentIsoTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    std::stringstream ss;
    ss << std::put_time(&tmLocal, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Simplified stub; your Python version currently doesn't use this because the
// CPU scan is commented out.
void getAppWithMaxCpuUsage(std::string &exeName, double &cpuUsage) {
    exeName.clear();
    cpuUsage = 0.0;
    // If you ever want this enabled, you'll need to sample process CPU times
    // via GetProcessTimes / PDH, similar to psutil's behavior.
}

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

// Expand "~" to user home directory (rough equivalent of os.path.expanduser("~"))
std::string getHomeDir() {
    const char *userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) {
        return userProfile;
    }
    // Fallback to current directory
    return ".";
}

void logTask(const std::string &machineId) {
    bool plugged = false;
    int percent = -1;
    if (!getBatteryStatus(plugged, percent)) {
        // If we can't get battery status, just return; you can also log an error if you want.
        return;
    }

    // CPU process info currently disabled, same as the Python code.
    std::string home = getHomeDir();
    std::string path = home + "\\batteryLog.csv";

    namespace fs = std::filesystem;
    bool fileExists = fs::exists(path);

    std::ofstream ofs(path, std::ios::app);
    if (!ofs.is_open()) {
        return;
    }

    if (!fileExists) {
        ofs << "time,plugged,percent,machine_id\n";
    }

    ofs << getCurrentIsoTime() << ','
            << (plugged ? "True" : "False") << ','
            << percent << ','
            << machineId
            << "\n";
    ofs.flush();
}

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

std::string machineId;

#define TIMER_LOG 0x1525

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            logTask(machineId);
            SetTimer(hwnd, TIMER_LOG, 60000, nullptr);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_LOG);
            PostQuitMessage(0);
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_LOG) {
                logTask(machineId);
            }
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!ensureSingleInstance()) {
        return 0; // silently exit if another instance exists
    }

    machineId = getMachineId();

    const wchar_t CLASS_NAME[] = L"BatteryLoggerHiddenWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, // Optional window styles.
        CLASS_NAME, // Window class
        L"Battery Logger", // Window text
        WS_OVERLAPPEDWINDOW, // Window style

        // Position and size
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        nullptr, // Parent window
        nullptr, // Menu
        hInstance, // Instance handle
        nullptr // Additional application data
    );

    if (hwnd == nullptr) {
        return 0;
    }

    // Do not show the window.
    // ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

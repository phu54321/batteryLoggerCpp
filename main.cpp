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


// battery_logger.cpp
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shellapi.h>
#include <iphlpapi.h>
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
#include "resource.h"

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
std::wstring getHomeDir() {
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

std::wstring getTimestampForFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    std::wstringstream ss;
    ss << std::put_time(&tmLocal, L"%Y%m%d-%H%M%S");
    return ss.str();
}

std::string readWholeFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string loadTextResource(HINSTANCE hInstance, int resourceId) {
    HRSRC resource = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) {
        return {};
    }

    HGLOBAL loadedResource = LoadResource(hInstance, resource);
    if (!loadedResource) {
        return {};
    }

    DWORD size = SizeofResource(hInstance, resource);
    const auto *data = static_cast<const char *>(LockResource(loadedResource));
    if (!data || size == 0) {
        return {};
    }

    return std::string(data, data + size);
}

std::string escapeForRawTemplateLiteral(const std::string &text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '`') {
            escaped += "\\`";
        } else {
            escaped += c;
        }
    }

    return escaped;
}

bool replaceFirst(std::string &text, const std::string &needle, const std::string &replacement) {
    size_t pos = text.find(needle);
    if (pos == std::string::npos) {
        return false;
    }

    text.replace(pos, needle.size(), replacement);
    return true;
}

std::filesystem::path createWebLogReport(HINSTANCE hInstance) {
    std::string html = loadTextResource(hInstance, IDR_WEBUI_HTML);
    if (html.empty()) {
        return {};
    }

    std::string csv = readWholeFile(getLogPath());
    if (csv.empty()) {
        return {};
    }

    const std::string marker = "<<<<<>>>>>";
    const std::string replacement = "<<<<<" + escapeForRawTemplateLiteral(csv) + ">>>>>";
    if (!replaceFirst(html, marker, replacement)) {
        return {};
    }

    wchar_t tempPathBuffer[MAX_PATH + 1] = {};
    DWORD tempPathLength = GetTempPathW(MAX_PATH + 1, tempPathBuffer);
    if (tempPathLength == 0 || tempPathLength > MAX_PATH) {
        return {};
    }

    std::filesystem::path outputPath = std::filesystem::path(tempPathBuffer) /
                                       (L"batteryLog-" + getTimestampForFilename() + L".html");

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        return {};
    }

    output.write(html.data(), static_cast<std::streamsize>(html.size()));
    if (!output) {
        return {};
    }

    return outputPath;
}

void logTask(const std::string &machineId) {
    bool plugged = false;
    int percent = -1;
    if (!getBatteryStatus(plugged, percent)) {
        // If we can't get battery status, just return; you can also log an error if you want.
        return;
    }

    // CPU process info currently disabled, same as the Python code.
    auto logPath = getLogPath();
    namespace fs = std::filesystem;
    bool fileExists = fs::exists(logPath);

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
UINT g_wmTaskbarCreated = 0;

#define TIMER_LOG 0x1525
#define WM_TRAYICON (WM_USER + 1)
#define IDM_QUIT 1001
#define IDM_OPENLOG 1002
#define IDM_ABOUT 1003

void AddTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wcscpy_s(nid.szTip, L"batteryLogger");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenuW(hMenu, MF_STRING, IDM_OPENLOG, L"Open battery log");
    AppendMenuW(hMenu, MF_STRING, IDM_QUIT, L"Quit");

    SetForegroundWindow(hwnd);

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

HWND g_aboutDlg = nullptr;

static INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto hInstance = (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
            return TRUE;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            g_aboutDlg = nullptr;
            return TRUE;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == ID_ABOUTDLG_OK) {
                DestroyWindow(hwnd);
                g_aboutDlg = nullptr;
                return TRUE;
            }

            if (id == ID_ABOUTDLG_GOTOREPO) {
                ShellExecuteW(hwnd, L"open", L"https://github.com/phu54321/batteryLoggerCpp", nullptr, nullptr,
                              SW_SHOWNORMAL);
                return TRUE;
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == g_wmTaskbarCreated) {
        AddTrayIcon(hwnd, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
        return 0;
    }

    switch (uMsg) {
        case WM_CREATE:
            AddTrayIcon(hwnd, ((LPCREATESTRUCT) lParam)->hInstance);
            logTask(machineId);
            SetTimer(hwnd, TIMER_LOG, 60000, nullptr);
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon(hwnd);
            KillTimer(hwnd, TIMER_LOG);
            PostQuitMessage(0);
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_LOG) {
                logTask(machineId);
            }
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowContextMenu(hwnd);
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_QUIT:
                    DestroyWindow(hwnd);
                    break;

                case IDM_OPENLOG:
                {
                    auto hInstance = (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                    std::filesystem::path reportPath = createWebLogReport(hInstance);
                    if (reportPath.empty()) {
                        MessageBoxW(
                            hwnd,
                            L"Could not create the battery log report.",
                            L"batteryLoggerCpp",
                            MB_OK | MB_ICONERROR
                        );
                        break;
                    }

                    ShellExecuteW(hwnd, L"open", reportPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                }

                case IDM_ABOUT:
                    if (g_aboutDlg == nullptr) {
                        auto hInstance = (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                        g_aboutDlg = CreateDialog(
                            hInstance,
                            MAKEINTRESOURCE(IDD_ABOUT),
                            hwnd,
                            AboutDlgProc);
                    }
                    break;
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

    SetProcessDPIAware();

    machineId = getMachineId();
    g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

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
        if (g_aboutDlg && IsDialogMessage(g_aboutDlg, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

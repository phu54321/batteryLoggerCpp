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
#include <string>
#include "utils/aboutDialog.h"
#include "utils/appConstants.h"
#include "utils/batteryLog.h"
#include "utils/machineId.h"
#include "utils/singleInstance.h"
#include "utils/startupCheck.h"
#include "utils/trayIcon.h"
#include "utils/webLogReport.h"

std::string machineId;
UINT g_wmTaskbarCreated = 0;

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
                    std::wstring reportPath = createWebLogReport(hInstance, getLogPath());
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
                    showAboutDialog(hwnd);
                    break;
            }
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();

    if (copyToStartupAndRestartIfNeeded()) {
        return 0;
    }

    if (!ensureSingleInstance()) {
        return 0; // silently exit if another instance exists
    }


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
        if (handleAboutDialogMessage(&msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

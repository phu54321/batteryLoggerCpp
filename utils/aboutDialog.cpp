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

#include "aboutDialog.h"
#include "../resource.h"

#include <shellapi.h>

static HWND g_aboutDlg = nullptr;

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

void showAboutDialog(HWND owner) {
    if (g_aboutDlg != nullptr) {
        return;
    }

    auto hInstance = (HINSTANCE) GetWindowLongPtr(owner, GWLP_HINSTANCE);
    g_aboutDlg = CreateDialog(
        hInstance,
        MAKEINTRESOURCE(IDD_ABOUT),
        owner,
        AboutDlgProc);
}

bool handleAboutDialogMessage(MSG *msg) {
    return g_aboutDlg && IsDialogMessage(g_aboutDlg, msg);
}

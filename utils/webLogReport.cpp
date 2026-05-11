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

#include "webLogReport.h"
#include "../resource.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

static std::wstring getTimestampForFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
    localtime_s(&tmLocal, &tt);

    std::wstringstream ss;
    ss << std::put_time(&tmLocal, L"%Y%m%d-%H%M%S");
    return ss.str();
}

static std::string readWholeFile(const std::wstring &path) {
    FILE *file = _wfopen(path.c_str(), L"rb");
    if (!file) {
        return {};
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return {};
    }
    fseek(file, 0, SEEK_SET);

    std::string text;
    text.resize(static_cast<size_t>(size));
    if (size > 0 && fread(text.data(), 1, static_cast<size_t>(size), file) != static_cast<size_t>(size)) {
        fclose(file);
        return {};
    }

    fclose(file);
    return text;
}

static bool writeWholeFile(const std::wstring &path, const std::string &text) {
    FILE *file = _wfopen(path.c_str(), L"wb");
    if (!file) {
        return false;
    }

    bool ok = fwrite(text.data(), 1, text.size(), file) == text.size();
    fclose(file);
    return ok;
}

static std::string loadTextResource(HINSTANCE hInstance, int resourceId) {
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

static std::string escapeForRawTemplateLiteral(const std::string &text) {
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

static bool replaceFirst(std::string &text, const std::string &needle, const std::string &replacement) {
    size_t pos = text.find(needle);
    if (pos == std::string::npos) {
        return false;
    }

    text.replace(pos, needle.size(), replacement);
    return true;
}

static std::wstring joinPath(const std::wstring &dir, const std::wstring &filename) {
    if (dir.empty() || dir.back() == L'\\' || dir.back() == L'/') {
        return dir + filename;
    }

    return dir + L"\\" + filename;
}

std::wstring createWebLogReport(HINSTANCE hInstance, const std::wstring &logPath) {
    std::string html = loadTextResource(hInstance, IDR_WEBUI_HTML);
    if (html.empty()) {
        return {};
    }

    std::string csv = readWholeFile(logPath);
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

    std::wstring outputPath = joinPath(
        tempPathBuffer,
        L"batteryLog-" + getTimestampForFilename() + L".html"
    );

    if (!writeWholeFile(outputPath, html)) {
        return {};
    }

    return outputPath;
}

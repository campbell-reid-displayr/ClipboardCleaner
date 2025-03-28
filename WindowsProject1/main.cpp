#include <windows.h>
#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>
#include <wininet.h>
#include <WinUser.h>

#include "json.hpp"
using json = nlohmann::json;

#pragma comment(lib, "wininet.lib")

#define WM_CLIPBOARDUPDATE 0x031D

HWND hwnd;

// --- CONFIG STRUCT ---
struct Config {
    std::string baseUrl;
    std::string longUrlPath;

    static Config LoadFromFile(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) throw std::runtime_error("Could not open config file");
        json j;
        in >> j;

        Config cfg;
        cfg.baseUrl = j["baseUrl"];
        cfg.longUrlPath = j["longUrlPath"];
        return cfg;
    }

    std::string getLongUrlPrefix() const {
        return baseUrl + longUrlPath;
    }
};

Config g_config;
const std::string kShortUrlSuffix = "/:";

std::string getShortUrlPrefix() {
    return g_config.baseUrl + kShortUrlSuffix;
}

// --- STRING CONVERSIONS ---
std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

std::wstring stringToWstring(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

std::string urlDecode(const std::string& src) {
    std::string ret;
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] == '%' && i + 2 < src.size()) {
            int hex = 0;
            std::istringstream(src.substr(i + 1, 2)) >> std::hex >> hex;
            ret.push_back(static_cast<char>(hex));
            i += 2;
        }
        else if (src[i] == '+') {
            ret.push_back(' ');
        }
        else {
            ret.push_back(src[i]);
        }
    }
    return ret;
}

// --- FOLLOW REDIRECT ---
std::string FollowRedirect(const std::string& url) {
    std::wstring userAgent = stringToWstring("ClipboardMonitor");
    HINTERNET hSession = InternetOpen(userAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hSession) return url;

    std::wstring wideUrl = stringToWstring(url);
    DWORD flags = INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_RELOAD;
    HINTERNET hUrl = InternetOpenUrl(hSession, wideUrl.c_str(), NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hSession);
        return url;
    }

    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL)) {
        if (statusCode >= 300 && statusCode < 400) {
            wchar_t location[2048];
            DWORD locationSize = sizeof(location);
            if (HttpQueryInfo(hUrl, HTTP_QUERY_LOCATION, location, &locationSize, NULL)) {
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hSession);
                return FollowRedirect(wstringToString(location));
            }
        }
    }

    wchar_t finalUrl[2048];
    DWORD finalUrlSize = sizeof(finalUrl);
    if (InternetQueryOption(hUrl, INTERNET_OPTION_URL, finalUrl, &finalUrlSize)) {
        std::string result = wstringToString(finalUrl);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hSession);
        return result;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hSession);
    return url;
}

// --- CLIPBOARD MODIFIER ---
std::string ModifyClipboardContent(const std::string& original) {
    std::string urlToProcess = original;
    if (original.find(getShortUrlPrefix()) == 0) {
        urlToProcess = FollowRedirect(original);
    }

    if (urlToProcess.substr(0, g_config.getLongUrlPrefix().size()) != g_config.getLongUrlPrefix()) {
        return original;
    }

    size_t queryPos = urlToProcess.find('?');
    if (queryPos == std::string::npos) return original;

    std::string queryString = urlToProcess.substr(queryPos + 1);
    std::regex pattern(R"(id=([^&]+))");
    std::smatch match;
    if (std::regex_search(queryString, match, pattern)) {
        std::string decodedId = urlDecode(match[1].str());
        return g_config.baseUrl + decodedId;
    }

    return original;
}

// --- CLIPBOARD SETTERS ---
void SetClipboardText(const std::string& text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hGlob) {
            char* pData = static_cast<char*>(GlobalLock(hGlob));
            strcpy_s(pData, text.size() + 1, text.c_str());
            GlobalUnlock(hGlob);
            SetClipboardData(CF_TEXT, hGlob);
        }
        CloseClipboard();
    }
}

void SetClipboardText(const std::wstring& text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        size_t sizeInBytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
        if (hGlob) {
            wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hGlob));
            wcscpy_s(pData, text.size() + 1, text.c_str());
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
        CloseClipboard();
    }
}

// --- WINDOW CALLBACK ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CLIPBOARDUPDATE: {
        if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(hwnd)) {
            HGLOBAL hGlob = GetClipboardData(CF_UNICODETEXT);
            if (hGlob) {
                wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hGlob));
                if (pData) {
                    std::string original = wstringToString(pData);
                    std::string modified = ModifyClipboardContent(original);
                    if (modified != original) {
                        SetClipboardText(stringToWstring(modified));
                    }
                    GlobalUnlock(hGlob);
                }
            }
            CloseClipboard();
        }
        else if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(hwnd)) {
            HGLOBAL hGlob = GetClipboardData(CF_TEXT);
            if (hGlob) {
                char* pData = static_cast<char*>(GlobalLock(hGlob));
                if (pData) {
                    std::string original(pData);
                    std::string modified = ModifyClipboardContent(original);
                    if (modified != original) {
                        SetClipboardText(modified);
                    }
                    GlobalUnlock(hGlob);
                }
            }
            CloseClipboard();
        }
        break;
    }
    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

// --- WINDOW CREATION ---
HWND CreateMessageWindow(HINSTANCE hInstance) {
    const char CLASS_NAME[] = "ClipboardMonitorClass";
    wchar_t wClassName[256];
    MultiByteToWideChar(CP_ACP, 0, CLASS_NAME, -1, wClassName, 256);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = wClassName;

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0,
        wClassName,
        L"Clipboard Monitor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(hwnd, SW_HIDE);
    return hwnd;
}

// --- MAIN ---
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    try {
        g_config = Config::LoadFromFile("config.json");
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Config Load Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateMessageWindow(hInstance);
    if (!hwnd) return 1;

    if (!AddClipboardFormatListener(hwnd)) {
        MessageBoxA(nullptr, "Failed to register clipboard listener.", "Error", MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

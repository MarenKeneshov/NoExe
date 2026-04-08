#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <locale>
#include <codecvt>

#pragma comment(lib, "shell32.lib")

#define IDC_LIST 101
#define IDC_EDIT 102
#define IDC_ADD 103
#define IDC_REMOVE 104

HINSTANCE g_hInst = nullptr;
HWND g_hList = nullptr;
HWND g_hEdit = nullptr;
std::vector<std::wstring> g_blocked;
std::wstring g_logPath;

// Получаем путь к %appdata%\NoExe
std::wstring GetAppDataPath()
{
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    return std::wstring(path) + L"\\NoExe";
}

// Чтение списка из blocked.txt (UTF-16 LE)
void LoadLog()
{
    g_blocked.clear();
    std::wifstream f(g_logPath, std::ios::binary);
    if (!f) return;

    f.imbue(std::locale(f.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>));

    std::wstring line;
    while (std::getline(f, line))
    {
        if (!line.empty()) g_blocked.push_back(line);
    }
}

// Запись списка в blocked.txt (UTF-16 LE)
void SaveLog()
{
    std::wofstream f(g_logPath, std::ios::binary | std::ios::trunc);
    if (!f) return;

    f.imbue(std::locale(f.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>));

    for (const auto& line : g_blocked)
    {
        f << line << L"\n";
    }
}

void UpdateListBox()
{
    SendMessageW(g_hList, LB_RESETCONTENT, 0, 0);
    for (const auto& p : g_blocked)
        SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
}

// Создание IFEO для блокировки процесса
bool BlockProcess(const std::wstring& exeName)
{
    HKEY hKey;
    std::wstring subKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + exeName;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        const wchar_t* debugger = L"cmd.exe /k echo This process has been blocked by NoExe protection. All information is available from the author of the NoExe program. Contact link: https://github.com/MarenKeneshov";
        RegSetValueExW(hKey, L"Debugger", 0, REG_SZ, (const BYTE*)debugger,
            (DWORD)((wcslen(debugger) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// Удаление IFEO для разблокировки
bool UnblockProcess(const std::wstring& exeName)
{
    std::wstring subKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + exeName;
    return RegDeleteTreeW(HKEY_LOCAL_MACHINE, subKey.c_str()) == ERROR_SUCCESS;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // Создаём шрифт Segoe UI, он самый адекватный и красивый, и крутой, и свэговый
        HFONT hFont = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI"
        );

        g_hList = CreateWindowW(L"LISTBOX", NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
            10, 10, 360, 200, hwnd, (HMENU)IDC_LIST, g_hInst, NULL);
        SendMessageW(g_hList, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hEdit = CreateWindowW(L"EDIT", NULL,
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            10, 220, 240, 25, hwnd, (HMENU)IDC_EDIT, g_hInst, NULL);
        SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnAdd = CreateWindowW(L"BUTTON", L"+", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            260, 220, 50, 25, hwnd, (HMENU)IDC_ADD, g_hInst, NULL);
        SendMessageW(hBtnAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnRemove = CreateWindowW(L"BUTTON", L"-", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            320, 220, 50, 25, hwnd, (HMENU)IDC_REMOVE, g_hInst, NULL);
        SendMessageW(hBtnRemove, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hStatic1 = CreateWindowW(L"STATIC", L"Coded by Keneshov Maren", WS_VISIBLE | WS_CHILD,
            10, 260, 360, 20, hwnd, nullptr, g_hInst, NULL);
        SendMessageW(hStatic1, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hStatic2 = CreateWindowW(L"STATIC", L"YouTube: Maren 704", WS_VISIBLE | WS_CHILD,
            10, 280, 360, 20, hwnd, nullptr, g_hInst, NULL);
        SendMessageW(hStatic2, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hStatic3 = CreateWindowW(L"STATIC", L"GitHub: MarenKeneshov", WS_VISIBLE | WS_CHILD,
            10, 300, 360, 20, hwnd, nullptr, g_hInst, NULL);
        SendMessageW(hStatic3, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Создаём папку и лог-файл
        std::wstring folder = GetAppDataPath();
        CreateDirectoryW(folder.c_str(), NULL);
        g_logPath = folder + L"\\blocked.txt";
        std::wofstream f(g_logPath, std::ios::app);
        f.close();

        LoadLog();
        UpdateListBox();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ADD:
        {
            wchar_t buf[256] = {};
            GetWindowTextW(g_hEdit, buf, 256);
            std::wstring exe = buf;
            if (!exe.empty() && std::find(g_blocked.begin(), g_blocked.end(), exe) == g_blocked.end())
            {
                if (BlockProcess(exe))
                {
                    g_blocked.push_back(exe);
                    SaveLog();
                    UpdateListBox();
                }
            }
            SetWindowTextW(g_hEdit, L"");
            break;
        }
        case IDC_REMOVE:
        {
            int sel = (int)SendMessageW(g_hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel < (int)g_blocked.size())
            {
                std::wstring exe = g_blocked[sel];
                if (UnblockProcess(exe))
                {
                    g_blocked.erase(g_blocked.begin() + sel);
                    SaveLog();
                    UpdateListBox();
                }
            }
            break;
        }
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    g_hInst = hInstance;

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NoExeBlocker";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"NoExeBlocker", L"NoExe Blocker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 370,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

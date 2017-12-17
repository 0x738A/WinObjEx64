/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2015 - 2017
*
*  TITLE:       SUP.H
*
*  VERSION:     1.51
*
*  DATE:        02 Dec 2017
*
*  Common header file for the program support routines.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#pragma once

#include <cfgmgr32.h>
#include <setupapi.h>

typedef struct _SAPIDBOBJ {
    LIST_ENTRY			sapiDBHead;
    HDEVINFO			hDevInfo;
    CRITICAL_SECTION	objCS;
} SAPIDBOBJ, *PSAPIDBOBJ;

typedef struct _ENUMICONINFO {
    HICON hIcon;
    INT cx, cy;
} ENUMICONINFO, *PENUMICONINFO;

//
// Gripper window size
//
#define GRIPPER_SIZE 11

//
// Type icons starts from 300
//
#define ID_FROM_VALUE(id) ((id) - 300)
#define ID_TO_VALUE(id) ((id) + 300)

#define GET_BIT(Integer, Bit) (((Integer) >> (Bit)) & 0x1)
#define SET_BIT(Integer, Bit) ((Integer) |= 1 << (Bit))
#define CLEAR_BIT(Integer, Bit) ((Integer) &= ~(1 << (Bit)))

//
// Conversion buffer size
//
#define DBUFFER_SIZE                 512

typedef struct _LANGANDCODEPAGE {
    WORD wLanguage;
    WORD wCodePage;
} LANGANDCODEPAGE, *LPTRANSLATE;

typedef struct _ENUM_PARAMS {
    PCWSTR	lpSubDirName;
    PVOID	scmSnapshot;
    SIZE_T	scmNumberOfEntries;
    PVOID	sapiDB;
} ENUM_PARAMS, *PENUM_PARAMS;

typedef struct _SAPIDBENTRY {
    LIST_ENTRY ListEntry;
    LPWSTR lpDeviceName;
    LPWSTR lpDeviceDesc;
} SAPIDBENTRY, *PSAPIDBENTRY;

extern ENUM_PARAMS	g_enumParams;
extern POBJECT_TYPES_INFORMATION g_pObjectTypesInfo;

BOOL supInitTreeListForDump(
    _In_  HWND  hwndParent,
    _Inout_ ATOM *pTreeListAtom,
    _Inout_ HWND *pTreeListHwnd
);

VOID supShowHelp(
    VOID
);

BOOL supQueryObjectFromHandle(
    _In_ HANDLE hOject,
    _Inout_ ULONG_PTR *Address,
    _Inout_opt_ UCHAR *TypeIndex
);

HICON supGetMainIcon(
    _In_ LPWSTR lpFileName,
    _In_ INT cx,
    _In_ INT cy
);

void supCopyMemory(
    _Inout_ void *dest,
    _In_ size_t ccdest,
    _In_ const void *src,
    _In_ size_t ccsrc
);

BOOL supUserIsFullAdmin(
    VOID
);

BOOL supIsSymlink(
    INT iItem
);

VOID supCenterWindow(
    HWND hwnd
);

VOID supSetWaitCursor(
    BOOL fSet
);

HIMAGELIST supLoadImageList(
    HINSTANCE hInst,
    UINT FirstId,
    UINT LastId
);

UINT supGetObjectIndexByTypeName(
    _In_ LPCWSTR lpTypeName
);

UINT supGetObjectNameIndexByTypeIndex(
    _In_ PVOID Object,
    _In_ UCHAR TypeIndex
);

VOID supRunAsAdmin(
    VOID
);

VOID supSetMenuIcon(
    HMENU hMenu,
    UINT Item,
    ULONG_PTR IconData
);

VOID supHandleObjectPopupMenu(
    HWND hwnd,
    int iItem,
    LPPOINT point
);

VOID supHandleTreePopupMenu(
    HWND hwnd,
    LPPOINT point
);

VOID supCreateToolbarButtons(
    HWND hWndToolbar
);

VOID supInit(
    BOOL IsFullAdmin
);

VOID supShutdown(
    VOID
);

PVOID supGetObjectTypesInfo(
    VOID
);

VOID supShowProperties(
    _In_ HWND hwndDlg,
    _In_ LPWSTR lpFileName
);

VOID supClipboardCopy(
    _In_ LPWSTR lpText,
    _In_ SIZE_T cbText
);

BOOL supEnablePrivilege(
    _In_ DWORD	PrivilegeName,
    _In_ BOOL	fEnable
);

BOOL WINAPI supEnumEnableChildWindows(
    _In_  HWND hwnd,
    _In_  LPARAM lParam
);

BOOL WINAPI supEnumHideChildWindows(
    _In_  HWND hwnd,
    _In_  LPARAM lParam
);

LPWSTR supGetItemText(
    _In_ HWND ListView,
    _In_ INT nItem,
    _In_ INT nSubItem,
    _Inout_opt_ PSIZE_T lpSize
);

BOOL supQueryLinkTarget(
    _In_opt_	HANDLE hRootDirectory,
    _In_		PUNICODE_STRING ObjectName,
    _Inout_		LPWSTR Buffer,
    _In_		DWORD cbBuffer
);

BOOL supQuerySectionFileInfo(
    _In_opt_	HANDLE hRootDirectory,
    _In_		PUNICODE_STRING ObjectName,
    _Inout_		LPWSTR Buffer,
    _In_		DWORD ccBuffer
);

BOOL supQueryTypeInfo(
    _In_	LPWSTR lpTypeName,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

BOOL supQueryDriverDescription(
    _In_	LPWSTR lpDriverName,
    _In_	PVOID scmSnapshot,
    _In_	SIZE_T	scmNumberOfEntries,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

BOOL supQueryDeviceDescription(
    _In_	LPWSTR lpDeviceName,
    _In_	PVOID Snapshot,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

BOOL supQueryWinstationDescription(
    _In_	LPWSTR lpWindowStationName,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

BOOL supQueryProcessName(
    _In_	DWORD dwProcessId,
    _In_	PVOID ProcessList,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

BOOL supFindModuleNameByAddress(
    _In_	PRTL_PROCESS_MODULES pModulesList,
    _In_	PVOID Address,
    _Inout_	LPWSTR Buffer,
    _In_	DWORD ccBuffer
);

ULONG supFindModuleEntryByAddress(
    _In_	PRTL_PROCESS_MODULES pModulesList,
    _In_	PVOID Address
);

PVOID supGetSystemInfo(
    _In_ SYSTEM_INFORMATION_CLASS InfoClass
);

HANDLE supOpenDirectory(
    _In_ LPWSTR lpDirectory
);

HANDLE supOpenDirectoryForObject(
    _In_ LPWSTR lpObjectName,
    _In_ LPWSTR lpDirectory
);

BOOL supDumpSyscallTableConverted(
    _In_ PKLDBGCONTEXT Context,
    _Inout_ PUTable *Table
);

PVOID supCreateSCMSnapshot(
    PSIZE_T lpNumberOfEntries
);

PVOID sapiCreateSetupDBSnapshot(
    VOID
);

VOID supFreeSCMSnapshot(
    _In_ PVOID Snapshot
);

VOID sapiFreeSnapshot(
    _In_ PVOID Snapshot
);

VOID supQueryKnownDlls(
    VOID
);

BOOL supSaveDialogExecute(
    _In_ HWND OwnerWindow,
    _Inout_ LPWSTR SaveFileName,
    _In_ LPWSTR lpDialogFilter
);

ULONG_PTR supWriteBufferToFile(
    _In_ PWSTR lpFileName,
    _In_ PVOID Buffer,
    _In_ SIZE_T Size,
    _In_ BOOL Flush,
    _In_ BOOL Append
);

HWND supCreateSzGripWindow(
    _In_ HWND hwndOwner
);

VOID supSzGripWindowOnResize(
    _In_ HWND hwndOwner,
    _In_ HWND hwndSizeGrip
);

BOOL supIsProcess32bit(
    _In_ HANDLE hProcess
);

ULONG_PTR supQuerySystemRangeStart(
    VOID
);

BOOL supGetWin32FileName(
    _In_ LPWSTR FileName,
    _Inout_ LPWSTR Win32FileName,
    _In_ SIZE_T ccWin32FileName
);

BOOL supIsWine(
    VOID
);

USHORT supIsAddressPrefix(
    _In_ LPWSTR lpName,
    _In_ SIZE_T cbName
);

#define PathFileExists(lpszPath) (GetFileAttributes(lpszPath) != (DWORD)-1)

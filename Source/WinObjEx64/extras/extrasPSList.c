/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2019
*
*  TITLE:       EXTRASPSLIST.C
*
*  VERSION:     1.73
*
*  DATE:        09 Mar 2019
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#define OEMRESOURCE
#include "global.h"
#include "propDlg.h"
#include "extras.h"
#include "extrasPSList.h"
#include "treelist/treelist.h"
#include "resource.h"

#define Y_SPLITTER_SIZE 4
#define Y_SPLITTER_MIN  80

#define T_IDLE_PROCESS TEXT("Idle")
#define T_IDLE_PROCESS_LENGTH sizeof(T_IDLE_PROCESS)

EXTRASCONTEXT   PsDlgContext;
PVOID           g_Snapshot = NULL;
static int      y_splitter_pos = 200, y_capture_pos = 0, y_splitter_max = 0;

PSYSTEM_PROCESSES_INFORMATION g_CurrentProcessEntry = NULL;

VOID CreateProcessTreeList();
VOID CreateThreadList(
    _In_ PSYSTEM_PROCESSES_INFORMATION ProcessEntry);

/*
* PsxListGetProcessEntryByProcessId
*
* Purpose:
*
* Return process entry from g_Snapshot for given process id.
*
*/
PVOID PsxListGetProcessEntryByProcessId(
    _In_ HANDLE UniqueProcessId)
{
    ULONG NextEntryDelta = 0;
    union {
        PSYSTEM_PROCESSES_INFORMATION ProcessEntry;
        PBYTE ListRef;
    } List;

    List.ListRef = (PBYTE)g_Snapshot;

    do {

        List.ListRef += NextEntryDelta;

        if (List.ProcessEntry->UniqueProcessId == UniqueProcessId) {
            return List.ProcessEntry;
        }

        NextEntryDelta = List.ProcessEntry->NextEntryDelta;

    } while (NextEntryDelta);

    return NULL;
}

/*
* PsListDialogResize
*
* Purpose:
*
* WM_SIZE handler.
*
*/
INT_PTR PsListDialogResize(
    VOID
)
{
    RECT r, szr;

    RtlSecureZeroMemory(&r, sizeof(RECT));
    RtlSecureZeroMemory(&szr, sizeof(RECT));

    SendMessage(PsDlgContext.StatusBar, WM_SIZE, 0, 0);
    GetClientRect(PsDlgContext.hwndDlg, &r);
    GetClientRect(PsDlgContext.StatusBar, &szr);
    y_splitter_max = r.bottom - Y_SPLITTER_MIN;

    SetWindowPos(PsDlgContext.TreeList, 0,
        0, 0,
        r.right,
        y_splitter_pos,
        SWP_NOOWNERZORDER);

    SetWindowPos(PsDlgContext.ListView, 0,
        0, y_splitter_pos + Y_SPLITTER_SIZE,
        r.right,
        r.bottom - y_splitter_pos - Y_SPLITTER_SIZE - szr.bottom,
        SWP_NOOWNERZORDER);

    return 1;
}

/*
* PsListHandlePopupMenu
*
* Purpose:
*
* Processes/threads list popup construction
*
*/
VOID PsListHandlePopupMenu(
    _In_ HWND hwndDlg,
    _In_ LPPOINT point,
    _In_ UINT itemCopy,
    _In_ UINT itemRefresh
)
{
    HMENU hMenu;

    hMenu = CreatePopupMenu();
    if (hMenu) {
        InsertMenu(hMenu, 0, MF_BYCOMMAND, itemCopy, T_COPYOBJECT);
        InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, 2, MF_BYCOMMAND, itemRefresh, T_VIEW_REFRESH);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, point->x, point->y, 0, hwndDlg, NULL);
        DestroyMenu(hMenu);
    }

}

/*
* PsListCompareFunc
*
* Purpose:
*
* Dialog listview comparer function.
*
*/
INT CALLBACK PsListCompareFunc(
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort
)
{
    INT nResult = 0;

    switch (lParamSort) {
    case 0: //TID
    case 1: //BasePriority
        return supGetMaxOfTwoULongFromString(
            PsDlgContext.ListView,
            lParam1,
            lParam2,
            PsDlgContext.lvColumnToSort,
            PsDlgContext.bInverseSort);
    case 2: //string (fixed size)
    case 5: //string (fixed size)
        return supGetMaxCompareTwoFixedStrings(
            PsDlgContext.ListView,
            lParam1,
            lParam2,
            PsDlgContext.lvColumnToSort,
            PsDlgContext.bInverseSort);
    case 3: //ethread (hex)
    case 4: //address (hex)
        return supGetMaxOfTwoU64FromHex(
            PsDlgContext.ListView,
            lParam1,
            lParam2,
            PsDlgContext.lvColumnToSort,
            PsDlgContext.bInverseSort);
    }

    return nResult;
}

/*
* PsListGetObjectEntry
*
* Purpose:
*
* Return pointer to data from selected object list entry.
*
*/
PVOID PsListGetObjectEntry(
    _In_ BOOL bTreeList)
{
    INT nSelected;
    TVITEMEX itemex;
    TL_SUBITEMS_FIXED *subitems = NULL;
    PVOID ObjectEntry = NULL;

    if (bTreeList) {
        RtlSecureZeroMemory(&itemex, sizeof(itemex));

        itemex.hItem = TreeList_GetSelection(PsDlgContext.TreeList);
        if (TreeList_GetTreeItem(PsDlgContext.TreeList, &itemex, &subitems))
            ObjectEntry = subitems->UserParam;
    }
    else {
        nSelected = ListView_GetSelectionMark(PsDlgContext.ListView);
        supGetListViewItemParam(PsDlgContext.ListView, nSelected, &ObjectEntry);
    }

    return ObjectEntry;
}

/*
* PsListHandleObjectProp
*
* Purpose:
*
* Show properties for selected object.
*
*/
VOID PsListHandleObjectProp(
    _In_ HWND hwndDlg,
    _In_ BOOL bProcessList,
    _In_ PVOID ObjectEntry)
{
    SIZE_T sz;
    LPWSTR lpName;
    PROP_UNNAMED_OBJECT_INFO UnnamedObject;
    PSYSTEM_PROCESSES_INFORMATION Process;
    CLIENT_ID ClientId;

    if (g_PsPropWindow != NULL)
        return;

    RtlSecureZeroMemory(&UnnamedObject, sizeof(PROP_UNNAMED_OBJECT_INFO));

    if (bProcessList) {
        UnnamedObject.Process = (PSYSTEM_PROCESSES_INFORMATION)ObjectEntry;
    }
    else {
        UnnamedObject.Thread = (PSYSTEM_THREAD_INFORMATION)ObjectEntry;
    }

    if (bProcessList) {
        Process = (PSYSTEM_PROCESSES_INFORMATION)ObjectEntry;
        ClientId.UniqueProcess = Process->UniqueProcessId;
        ClientId.UniqueThread = NULL;
        supQueryProcessObjectInformation(&ClientId, &UnnamedObject.ObjectAddress);
    }
    else {
        Process = (PSYSTEM_PROCESSES_INFORMATION)PsxListGetProcessEntryByProcessId(
            UnnamedObject.Thread->ClientId.UniqueProcess);

        supQueryThreadObjectInformation(&UnnamedObject.Thread->ClientId, NULL, &UnnamedObject.ObjectAddress);
    }

    if (Process == NULL) //no such process in the list.
        return;

    //
    // Create fake name for display.
    //
    sz = 1024 + Process->ImageName.Length;
    lpName = (LPWSTR)supHeapAlloc(sz);
    if (lpName == NULL)
        return;

    if (Process->ImageName.Length == 0) {
        if (Process->UniqueProcessId == NULL) {
            _strcpy(lpName, T_IDLE_PROCESS);
        }
        else {
            _strcpy(lpName, TEXT("UnknownProcess"));
        }
    }
    else {
        RtlCopyMemory(lpName,
            Process->ImageName.Buffer,
            Process->ImageName.Length);
    }
    _strcat(lpName, TEXT(" PID:"));
    ultostr(HandleToULong(Process->UniqueProcessId), _strend(lpName));

    if (!bProcessList) {
        _strcat(lpName, TEXT(" TID:"));
        ultostr(HandleToULong(UnnamedObject.Thread->ClientId.UniqueThread), _strend(lpName));
    }

    propCreateDialog(hwndDlg,
        lpName,
        (bProcessList) ? OBTYPE_NAME_PROCESS : OBTYPE_NAME_THREAD,
        NULL,
        NULL,
        &UnnamedObject);

    supHeapFree(lpName);
}

/*
* PsListHandleNotify
*
* Purpose:
*
* WM_NOTIFY processing for dialog listview.
*
*/
VOID PsListHandleNotify(
    _In_ HWND hwndDlg,
    _In_ LPARAM lParam
)
{
    LPNMHDR  nhdr = (LPNMHDR)lParam;
    INT      nImageIndex;

    HWND TreeControl;

    PSYSTEM_PROCESSES_INFORMATION ProcessEntry;
    PVOID ThreadEntry;

    if (nhdr == NULL)
        return;

    TreeControl = (HWND)TreeList_GetTreeControlWindow(PsDlgContext.TreeList);

    if (nhdr->hwndFrom == PsDlgContext.ListView) {

        switch (nhdr->code) {

        case NM_DBLCLK:
            ThreadEntry = PsListGetObjectEntry(FALSE);
            if (ThreadEntry) {
                PsListHandleObjectProp(hwndDlg, FALSE, ThreadEntry);
            }
            break;

        case LVN_COLUMNCLICK:
            PsDlgContext.bInverseSort = !PsDlgContext.bInverseSort;
            PsDlgContext.lvColumnToSort = ((NMLISTVIEW *)lParam)->iSubItem;

            ListView_SortItemsEx(PsDlgContext.ListView, &PsListCompareFunc, (LPARAM)PsDlgContext.lvColumnToSort);

            nImageIndex = ImageList_GetImageCount(g_ListViewImages);
            if (PsDlgContext.bInverseSort)
                nImageIndex -= 2;
            else
                nImageIndex -= 1;

            supUpdateLvColumnHeaderImage(
                PsDlgContext.ListView,
                PsDlgContext.lvColumnCount,
                PsDlgContext.lvColumnToSort,
                nImageIndex);

            break;

        default:
            break;
        }
    }
    else if (nhdr->hwndFrom == TreeControl) {

        switch (nhdr->code) {

        case NM_DBLCLK:
            ProcessEntry = (PSYSTEM_PROCESSES_INFORMATION)PsListGetObjectEntry(TRUE);
            if (ProcessEntry) {
                PsListHandleObjectProp(hwndDlg, TRUE, ProcessEntry);
            }
            break;

        case TVN_SELCHANGED:

            ProcessEntry = (PSYSTEM_PROCESSES_INFORMATION)PsListGetObjectEntry(TRUE);
            if (ProcessEntry) {
                ListView_DeleteAllItems(PsDlgContext.ListView);
                g_CurrentProcessEntry = ProcessEntry;
                CreateThreadList(ProcessEntry);
            }

            break;
        default:
            break;
        }

    }
}

/*
* PsListDialogProc
*
* Purpose:
*
* Drivers Dialog window procedure.
*
*/
INT_PTR CALLBACK PsListDialogProc(
    _In_  HWND hwndDlg,
    _In_  UINT uMsg,
    _In_  WPARAM wParam,
    _In_  LPARAM lParam
)
{
    INT dy;
    RECT crc;
    INT mark;
    HWND TreeListControl;

    switch (uMsg) {

    case WM_CONTEXTMENU:

        RtlSecureZeroMemory(&crc, sizeof(crc));

        TreeListControl = TreeList_GetTreeControlWindow(PsDlgContext.TreeList);

        if ((HWND)wParam == TreeListControl) {
            GetCursorPos((LPPOINT)&crc);
            PsListHandlePopupMenu(hwndDlg, (LPPOINT)&crc, ID_OBJECT_COPY, ID_VIEW_REFRESH);
        }

        if ((HWND)wParam == PsDlgContext.ListView) {

            mark = ListView_GetSelectionMark(PsDlgContext.ListView);

            if (lParam == MAKELPARAM(-1, -1)) {
                ListView_GetItemRect(PsDlgContext.ListView, mark, &crc, TRUE);
                crc.top = crc.bottom;
                ClientToScreen(PsDlgContext.ListView, (LPPOINT)&crc);
            }
            else
                GetCursorPos((LPPOINT)&crc);

            PsListHandlePopupMenu(hwndDlg, (LPPOINT)&crc, ID_OBJECT_COPY + 1, ID_VIEW_REFRESH + 1);
        }

        break;

    case WM_NOTIFY:
        PsListHandleNotify(hwndDlg, lParam);
        break;

    case WM_COMMAND:

        switch (LOWORD(wParam)) {
        case IDCANCEL:
            SendMessage(hwndDlg, WM_CLOSE, 0, 0);
            return TRUE;
        case ID_OBJECT_COPY:
        case ID_OBJECT_COPY + 1:
            if (LOWORD(wParam) == ID_OBJECT_COPY) {
                supCopyTreeListSubItemValue(PsDlgContext.TreeList, 0);
            }
            else {
                supCopyListViewSubItemValue(PsDlgContext.ListView, 3);
            }
            break;
        case ID_VIEW_REFRESH:
            TreeList_ClearTree(PsDlgContext.TreeList);
            ListView_DeleteAllItems(PsDlgContext.ListView);
            g_CurrentProcessEntry = NULL;
            CreateProcessTreeList();
            break;
        case ID_VIEW_REFRESH + 1:
            ListView_DeleteAllItems(PsDlgContext.ListView);
            CreateThreadList(g_CurrentProcessEntry);
            break;
        default:
            break;
        }
        break;

    case WM_INITDIALOG:
        supCenterWindow(hwndDlg);
        break;

    case WM_GETMINMAXINFO:
        if (lParam) {
            ((PMINMAXINFO)lParam)->ptMinTrackSize.x = 640;
            ((PMINMAXINFO)lParam)->ptMinTrackSize.y = 480;
        }
        break;

    case WM_SIZE:
        return PsListDialogResize();

    case WM_LBUTTONDOWN:
        SetCapture(hwndDlg);
        y_capture_pos = (int)(short)HIWORD(lParam);
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:

        if (wParam & MK_LBUTTON) {
            dy = (int)(short)HIWORD(lParam) - y_capture_pos;
            if (dy != 0) {
                y_capture_pos = (int)(short)HIWORD(lParam);
                y_splitter_pos += dy;
                if (y_splitter_pos < Y_SPLITTER_MIN)
                {
                    y_splitter_pos = Y_SPLITTER_MIN;
                    y_capture_pos = Y_SPLITTER_MIN;
                }

                if (y_splitter_pos > y_splitter_max)
                {
                    y_splitter_pos = y_splitter_max;
                    y_capture_pos = y_splitter_max;
                }
                SendMessage(hwndDlg, WM_SIZE, 0, 0);
            }
        }

        break;

    case WM_CLOSE:
        DestroyWindow(PsDlgContext.TreeList);
        DestroyWindow(hwndDlg);
        g_WinObj.AuxDialogs[wobjPsListDlgId] = NULL;
        if (g_Snapshot) {
            supHeapFree(g_Snapshot);
            g_Snapshot = NULL;
        }
        return TRUE;
    }

    return DefDlgProc(hwndDlg, uMsg, wParam, lParam);
}

/*
* PsListProcessInServicesList
*
* Purpose:
*
* Return TRUE if given process is in SCM snapshot.
*
*/
BOOLEAN PsListProcessInServicesList(
    _In_ HANDLE ProcessId,
    _In_ SCMDB *ServicesList
)
{
    DWORD u;
    LPENUM_SERVICE_STATUS_PROCESS pInfo = NULL;

    pInfo = (LPENUM_SERVICE_STATUS_PROCESS)ServicesList->Entries;
    for (u = 0; u < ServicesList->NumberOfEntries; u++) {
        if (pInfo[u].ServiceStatusProcess.dwProcessId)
            if (UlongToHandle(pInfo[u].ServiceStatusProcess.dwProcessId) == ProcessId)
            {
                return TRUE;
            }
    }
    return FALSE;
}

/*
* AddProcessEntryTreeList
*
* Purpose:
*
* Insert process entry to the treelist.
*
*/
HTREEITEM AddProcessEntryTreeList(
    _In_opt_ HTREEITEM RootItem,
    _In_ OBEX_PROCESS_LOOKUP_ENTRY* Entry,
    _In_opt_ PSYSTEM_HANDLE_INFORMATION_EX HandleList,
    _In_ SCMDB *ServicesList,
    _In_ PSID OurSid
)
{
    HTREEITEM hTreeItem = NULL;
    PSID ProcessSid;
    PSYSTEM_PROCESSES_INFORMATION processEntry;
    TL_SUBITEMS_FIXED subitems;

    ULONG_PTR ObjectAddress = 0;

    DWORD CurrentProcessId = GetCurrentProcessId();

    ULONG Length, r, fState = 0;
    PWSTR Caption, P, UserName = NULL;

    PROCESS_EXTENDED_BASIC_INFORMATION exbi;
    WCHAR szEPROCESS[32];

    SID SidLocalService = { SID_REVISION, 1, SECURITY_NT_AUTHORITY, { SECURITY_LOCAL_SERVICE_RID } };

    RtlSecureZeroMemory(&subitems, sizeof(subitems));

    //
    // Id + Name
    //
    processEntry = Entry->ProcessInformation;

    Length = 32;
    if (processEntry->ImageName.Length) {
        Length += processEntry->ImageName.Length;
    }
    else {
        if (processEntry->UniqueProcessId == 0) {
            Length += T_IDLE_PROCESS_LENGTH;
        }
    }

    Caption = (PWSTR)supHeapAlloc(Length);

    P = _strcat(Caption, TEXT("["));
    ultostr(HandleToULong(processEntry->UniqueProcessId), P);
    _strcat(Caption, TEXT("]"));

    _strcat(Caption, TEXT(" "));

    if (processEntry->UniqueProcessId == 0) {
        _strcat(Caption, T_IDLE_PROCESS);
    }
    else {
        _strcat(Caption, processEntry->ImageName.Buffer);
    }

    //
    // EPROCESS value (optional)
    //
    szEPROCESS[0] = 0;
    if (HandleList) {
        for (r = 0; r < HandleList->NumberOfHandles; r++)
            if (HandleList->Handles[r].UniqueProcessId == (ULONG_PTR)CurrentProcessId) {
                if (HandleList->Handles[r].HandleValue == (ULONG_PTR)Entry->hProcess) {
                    ObjectAddress = (ULONG_PTR)HandleList->Handles[r].Object;
                    break;
                }
            }

        if (ObjectAddress) {
            szEPROCESS[0] = L'0';
            szEPROCESS[1] = L'x';
            u64tohex(ObjectAddress, &szEPROCESS[2]);
        }
    }

    subitems.UserParam = (PVOID)Entry->EntryPtr;
    subitems.Count = 2;
    subitems.Text[0] = szEPROCESS;

    //
    // Colors.
    //
    //
    // 1. Services.
    //

    ProcessSid = supQueryProcessSid(Entry->hProcess);


    if (PsListProcessInServicesList(processEntry->UniqueProcessId, ServicesList) ||
        ((ProcessSid) && RtlEqualSid(&SidLocalService, ProcessSid)))
    {
        subitems.ColorFlags = TLF_BGCOLOR_SET;
        subitems.BgColor = 0xd0d0ff;
        fState = TVIF_STATE;
    }

    //
    // 2. Current user processes.
    //
    if (ProcessSid) {
        if (RtlEqualSid(OurSid, ProcessSid)) {
            subitems.ColorFlags = TLF_BGCOLOR_SET;
            subitems.BgColor = 0xffd0d0;
            fState = TVIF_STATE;
        }
    }

    //
    // 3. Store processes.
    //
    if (Entry->hProcess) {
        if (g_ExtApiSet.IsImmersiveProcess) {
            if (g_ExtApiSet.IsImmersiveProcess(Entry->hProcess)) {
                subitems.ColorFlags = TLF_BGCOLOR_SET;
                subitems.BgColor = 0xeaea00;
                fState = TVIF_STATE;
            }
        }
    }

    //
    // 4. Protected processes.
    //
    if (Entry->hProcess) {
        exbi.Size = sizeof(PROCESS_EXTENDED_BASIC_INFORMATION);
        if (NT_SUCCESS(NtQueryInformationProcess(Entry->hProcess, ProcessBasicInformation,
            &exbi, sizeof(exbi), &r)))
        {
            if (exbi.IsProtectedProcess) {
                subitems.ColorFlags = TLF_BGCOLOR_SET;
                subitems.BgColor = 0xe6ffe6;
                fState = TVIF_STATE;
            }
        }
    }

    //
    // User.
    //
    if (ProcessSid) {

        if (supLookupSidUserAndDomain(ProcessSid, &UserName)) {
            subitems.Text[1] = UserName;
        }
        supHeapFree(ProcessSid);
    }

    hTreeItem = TreeListAddItem(
        PsDlgContext.TreeList,
        RootItem,
        TVIF_TEXT | fState,
        0,
        0,
        Caption,
        &subitems);

    if (UserName)
        supHeapFree(UserName);

    return hTreeItem;
}

typedef BOOL(CALLBACK *FINDITEMCALLBACK)(
    HWND TreeList,
    HTREEITEM htItem,
    ULONG_PTR UserContext
    );

/*
* FindItemByProcessIdCallback
*
* Purpose:
*
* Search callback.
*
*/
BOOL CALLBACK FindItemMatchCallback(
    _In_ HWND TreeList,
    _In_ HTREEITEM htItem,
    _In_ ULONG_PTR UserContext
)
{
    HANDLE             ParentProcessId = (HANDLE)UserContext;
    TL_SUBITEMS_FIXED *subitems = NULL;
    TVITEMEX           itemex;

    PSYSTEM_PROCESSES_INFORMATION Entry;

    RtlSecureZeroMemory(&itemex, sizeof(itemex));
    itemex.hItem = htItem;
    TreeList_GetTreeItem(TreeList, &itemex, &subitems);

    if (subitems) {
        if (subitems->UserParam == NULL)
            return FALSE;

        Entry = (PSYSTEM_PROCESSES_INFORMATION)subitems->UserParam;
        return (ParentProcessId == Entry->UniqueProcessId);
    }

    return FALSE;
}

/*
* FindItemRecursive
*
* Purpose:
*
* Recursive find item.
*
*/
HTREEITEM FindItemRecursive(
    _In_ HWND TreeList,
    _In_ HTREEITEM htStart,
    _In_ FINDITEMCALLBACK FindItemCallback,
    _In_ ULONG_PTR UserContext
)
{
    HTREEITEM htItemMatch = NULL;
    HTREEITEM htItemCurrent = htStart;

    if (FindItemCallback == NULL)
        return NULL;

    while (htItemCurrent != NULL && htItemMatch == NULL) {
        if (FindItemCallback(TreeList, htItemCurrent, UserContext)) {
            htItemMatch = htItemCurrent;
        }
        else {
            htItemMatch = FindItemRecursive(TreeList,
                TreeList_GetChild(TreeList, htItemCurrent), FindItemCallback, UserContext);
        }
        htItemCurrent = TreeList_GetNextSibling(TreeList, htItemCurrent);
    }
    return htItemMatch;
}

/*
* FindParentItem
*
* Purpose:
*
* Return treelist item with given parent process id.
*
*/
HTREEITEM FindParentItem(
    _In_ HWND TreeList,
    _In_ HANDLE ParentProcessId
)
{
    HTREEITEM htiRoot = TreeList_GetRoot(TreeList);
    return FindItemRecursive(TreeList,
        htiRoot, FindItemMatchCallback, (ULONG_PTR)ParentProcessId);
}

//
// These constants missing in Windows SDK 8.1
//
#ifndef SERVICE_USER_SERVICE
#define SERVICE_USER_SERVICE           0x00000040
#endif

#ifndef SERVICE_USERSERVICE_INSTANCE
#define SERVICE_USERSERVICE_INSTANCE   0x00000080
#endif

/*
* CreateProcessTreeList
*
* Purpose:
*
* Build and output process tree list.
*
*/
VOID CreateProcessTreeList()
{
    DWORD ServiceEnumType;
    ULONG NextEntryDelta = 0, NumberOfProcesses = 0, NumberOfThreads = 0;

    HTREEITEM ViewRootHandle;

    HANDLE hProcess = NULL;
    PVOID InfoBuffer = NULL;
    PSYSTEM_HANDLE_INFORMATION_EX pHandles = NULL;
    PSID OurSid = NULL;

    OBEX_PROCESS_LOOKUP_ENTRY *spl = NULL, *LookupEntry;

    SCMDB ServicesList;

    OBJECT_ATTRIBUTES obja = RTL_INIT_OBJECT_ATTRIBUTES((PUNICODE_STRING)NULL, 0);

    WCHAR szBuffer[100];

    union {
        PSYSTEM_PROCESSES_INFORMATION ProcessEntry;
        PBYTE ListRef;
    } List;

    __try {
        ServicesList.NumberOfEntries = 0;
        ServicesList.Entries = NULL;

        OurSid = supQueryProcessSid(NtCurrentProcess());

        if (g_NtBuildNumber >= 14393) {
            ServiceEnumType = SERVICE_TYPE_ALL;
        }
        else if (g_NtBuildNumber >= 10240) {
            ServiceEnumType = SERVICE_WIN32 |
                SERVICE_ADAPTER |
                SERVICE_DRIVER |
                SERVICE_INTERACTIVE_PROCESS |
                SERVICE_USER_SERVICE |
                SERVICE_USERSERVICE_INSTANCE;
        }
        else {
            ServiceEnumType = SERVICE_DRIVER | SERVICE_WIN32;
        }
        if (!supCreateSCMSnapshot(ServiceEnumType, &ServicesList))
            __leave;

        if (g_Snapshot) {
            supHeapFree(g_Snapshot);
            g_Snapshot = NULL;
        }

        InfoBuffer = supGetSystemInfo(SystemProcessInformation);
        if (InfoBuffer == NULL)
            __leave;

        g_Snapshot = InfoBuffer;

        List.ListRef = (PBYTE)InfoBuffer;

        //
        // Calculate process handle list size.
        //
        do {

            List.ListRef += NextEntryDelta;

            if (List.ProcessEntry->ThreadCount) {
                NumberOfProcesses += 1;
                NumberOfThreads += List.ProcessEntry->ThreadCount;
            }

            NextEntryDelta = List.ProcessEntry->NextEntryDelta;

        } while (NextEntryDelta);

        //
        // Build process handle list.
        //
        spl = (OBEX_PROCESS_LOOKUP_ENTRY*)supHeapAlloc(NumberOfProcesses * sizeof(OBEX_PROCESS_LOOKUP_ENTRY));
        if (spl == NULL)
            __leave;

        LookupEntry = spl;

        NextEntryDelta = 0;
        List.ListRef = (PBYTE)InfoBuffer;

        do {
            List.ListRef += NextEntryDelta;
            hProcess = NULL;

            if (List.ProcessEntry->ThreadCount) {
                NtOpenProcess(
                    &hProcess,
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    &obja,
                    &List.ProcessEntry->Threads[0].ClientId);
            }

            LookupEntry->hProcess = hProcess;
            LookupEntry->EntryPtr = List.ListRef;
            LookupEntry = (OBEX_PROCESS_LOOKUP_ENTRY*)RtlOffsetToPointer(LookupEntry,
                sizeof(OBEX_PROCESS_LOOKUP_ENTRY));

            NextEntryDelta = List.ProcessEntry->NextEntryDelta;

        } while (NextEntryDelta);

        pHandles = (PSYSTEM_HANDLE_INFORMATION_EX)supGetSystemInfo(SystemExtendedHandleInformation);

        //
        // Output all process entries.
        //
        LookupEntry = spl;

        //
        // Show processes/threads count
        //
        _strcpy(szBuffer, TEXT("Processes: "));
        ultostr(NumberOfProcesses, _strend(szBuffer));
        SendMessage(PsDlgContext.StatusBar, SB_SETTEXT, 0, (LPARAM)&szBuffer);

        _strcpy(szBuffer, TEXT("Threads: "));
        ultostr(NumberOfThreads, _strend(szBuffer));
        SendMessage(PsDlgContext.StatusBar, SB_SETTEXT, 1, (LPARAM)&szBuffer);

        //idle
        AddProcessEntryTreeList(NULL,
            LookupEntry, pHandles, &ServicesList, OurSid);

        NumberOfProcesses--;
        ViewRootHandle = NULL;

        while (NumberOfProcesses) {

            LookupEntry = (OBEX_PROCESS_LOOKUP_ENTRY*)RtlOffsetToPointer(
                LookupEntry, sizeof(OBEX_PROCESS_LOOKUP_ENTRY));

            ViewRootHandle = FindParentItem(PsDlgContext.TreeList,
                LookupEntry->ProcessInformation->InheritedFromUniqueProcessId);

            if (ViewRootHandle == NULL) {
                ViewRootHandle = AddProcessEntryTreeList(NULL,
                    LookupEntry, pHandles, &ServicesList, OurSid);
            }
            else {
                AddProcessEntryTreeList(ViewRootHandle,
                    LookupEntry, pHandles, &ServicesList, OurSid);
            }

            if (LookupEntry->hProcess)
                NtClose(LookupEntry->hProcess);

            NumberOfProcesses--;
        }

    }
    __finally {
        if (OurSid) supHeapFree(OurSid);
        supFreeSCMSnapshot(&ServicesList);
        if (pHandles) supHeapFree(pHandles);
        if (spl) supHeapFree(spl);
    }
}

/*
* PsListGetThreadStateAsString
*
* Purpose:
*
* Return thread state string description.
*
*/
LPWSTR PsListGetThreadStateAsString(
    _In_ THREAD_STATE ThreadState,
    _In_ KWAIT_REASON WaitReason,
    _In_ LPWSTR StateBuffer)
{
    LPWSTR lpState = T_Unknown;
    LPWSTR lpWaitReason = T_Unknown;

    if (ThreadState == StateWait) {

        _strcpy(StateBuffer, TEXT("Wait:"));

        if (WaitReason < MAX_KNOWN_WAITREASON)
            lpWaitReason = T_WAITREASON[WaitReason];

        _strcat(StateBuffer, lpWaitReason);
    }
    else {


        switch (ThreadState) {
        case StateInitialized:
            lpState = TEXT("Initiailized");
            break;
        case StateReady:
            lpState = TEXT("Ready");
            break;
        case StateRunning:
            lpState = TEXT("Running");
            break;
        case StateStandby:
            lpState = TEXT("Standby");
            break;
        case StateTerminated:
            lpState = TEXT("Terminated");
            break;
        case StateTransition:
            lpState = TEXT("Transition");
            break;
        case StateUnknown:
        default:
            break;
        }

        _strcpy(StateBuffer, lpState);
    }
    return StateBuffer;
}

/*
* CreateThreadList
*
* Purpose:
*
* Build and output process threads list.
*
*/
VOID CreateThreadList(
    _In_ PSYSTEM_PROCESSES_INFORMATION ProcessEntry
)
{
    INT index;
    ULONG i, errorCount = 0;
    PSYSTEM_THREAD_INFORMATION ThreadEntry;
    PRTL_PROCESS_MODULES pModules;

    LVITEM lvitem;
    WCHAR szBuffer[MAX_PATH];

    ULONG_PTR startAddress = 0, objectPointer = 0;

    if (ProcessEntry == NULL)
        return;

    pModules = (PRTL_PROCESS_MODULES)supGetSystemInfo(SystemModuleInformation);

    for (i = 0, ThreadEntry = ProcessEntry->Threads;
        i < ProcessEntry->ThreadCount;
        i++, ThreadEntry++)
    {
        //
        // TID
        //
        RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
        ultostr(HandleToULong(ThreadEntry->ClientId.UniqueThread), szBuffer);

        RtlSecureZeroMemory(&lvitem, sizeof(lvitem));
        lvitem.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lvitem.iItem = MAXINT;
        lvitem.iImage = I_IMAGENONE;
        lvitem.pszText = szBuffer;
        lvitem.cchTextMax = MAX_PATH;
        lvitem.lParam = (LPARAM)ThreadEntry;
        index = ListView_InsertItem(PsDlgContext.ListView, &lvitem);

        //
        // Priority
        //
        szBuffer[0] = 0;
        ultostr(ThreadEntry->Priority, szBuffer);

        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem++;
        lvitem.pszText = szBuffer;
        lvitem.iItem = index;
        ListView_SetItem(PsDlgContext.ListView, &lvitem);

        //
        // State
        //
        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem++;

        lvitem.pszText = PsListGetThreadStateAsString(
            ThreadEntry->State,
            ThreadEntry->WaitReason, szBuffer);

        lvitem.iItem = index;
        ListView_SetItem(PsDlgContext.ListView, &lvitem);

        // Query thread specific information - object and win32 start address (need elevation).
        if (!supQueryThreadObjectInformation(
            &ThreadEntry->ClientId,
            &startAddress,
            &objectPointer))
        {
            errorCount += 1;
            if (startAddress == 0)
                startAddress = (ULONG_PTR)ThreadEntry->StartAddress;
        }

        //
        // ETHREAD
        //
        RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
        szBuffer[0] = TEXT('0');
        szBuffer[1] = TEXT('x');
        u64tohex(objectPointer, &szBuffer[2]);

        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem++;
        lvitem.pszText = szBuffer;
        lvitem.iItem = index;
        ListView_SetItem(PsDlgContext.ListView, &lvitem);

        //
        // StartAddress (either Win32StartAddress if possible or StartAddress from NtQSI)
        //
        RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
        szBuffer[0] = TEXT('0');
        szBuffer[1] = TEXT('x');

        u64tohex((ULONG_PTR)startAddress, &szBuffer[2]);

        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem++;
        lvitem.pszText = szBuffer;
        lvitem.iItem = index;
        ListView_SetItem(PsDlgContext.ListView, &lvitem);

        //
        // Module (for system threads)
        //
        RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
        if (startAddress > g_kdctx.SystemRangeStart) {
            if (!supFindModuleNameByAddress(
                pModules,
                (PVOID)startAddress,
                szBuffer,
                MAX_PATH))
            {
                _strcpy(szBuffer, T_Unknown);
            }
        }
        lvitem.mask = LVIF_TEXT;
        lvitem.iSubItem++;
        lvitem.pszText = szBuffer;
        lvitem.iItem = index;
        ListView_SetItem(PsDlgContext.ListView, &lvitem);
    }

    if (errorCount != 0) {
        _strcpy(szBuffer, TEXT("Some queries for threads information are failed"));
    }
    else {
        _strcpy(szBuffer, TEXT("All queries for threads information are succeeded"));
    }
    SendMessage(PsDlgContext.StatusBar, SB_SETTEXT, 2, (LPARAM)&szBuffer);

    ListView_SortItemsEx(
        PsDlgContext.ListView,
        PsListCompareFunc,
        PsDlgContext.lvColumnToSort);

    if (pModules) supHeapFree(pModules);
}

/*
* extrasCreatePsListDialog
*
* Purpose:
*
* Create and initialize Process List Dialog.
*
*/
VOID extrasCreatePsListDialog(
    _In_ HWND hwndParent
)
{
    HDITEM      hdritem;
    LVCOLUMN    col;
    WNDCLASSEX  wincls;

    INT SbParts[] = { 160, 320, -1 };

    //allow only one dialog
    if (g_WinObj.AuxDialogs[wobjPsListDlgId]) {
        if (IsIconic(g_WinObj.AuxDialogs[wobjPsListDlgId]))
            ShowWindow(g_WinObj.AuxDialogs[wobjPsListDlgId], SW_RESTORE);
        else
            SetActiveWindow(g_WinObj.AuxDialogs[wobjPsListDlgId]);
        return;
    }

    RtlSecureZeroMemory(&wincls, sizeof(wincls));
    wincls.cbSize = sizeof(WNDCLASSEX);
    wincls.lpfnWndProc = &PsListDialogProc;
    wincls.cbWndExtra = DLGWINDOWEXTRA;
    wincls.hInstance = g_WinObj.hInstance;
    wincls.hCursor = (HCURSOR)LoadImage(NULL,
        MAKEINTRESOURCE(OCR_SIZENS), IMAGE_CURSOR, 0, 0, LR_SHARED);
    wincls.hIcon = (HICON)LoadImage(g_WinObj.hInstance,
        MAKEINTRESOURCE(IDI_ICON_MAIN), IMAGE_ICON, 0, 0, LR_SHARED);
    wincls.lpszClassName = PSLISTCLASSNAME;

    RegisterClassEx(&wincls);

    RtlSecureZeroMemory(&PsDlgContext, sizeof(PsDlgContext));
    PsDlgContext.hwndDlg = CreateDialogParam(g_WinObj.hInstance,
        MAKEINTRESOURCE(IDD_DIALOG_PSLIST),
        hwndParent, NULL, 0);

    if (PsDlgContext.hwndDlg == NULL) {
        return;
    }

    if (g_kdctx.IsFullAdmin == FALSE) {
        SetWindowText(PsDlgContext.hwndDlg, TEXT("Processes (Non elevated mode, not all information can be queried)"));
    }

    g_WinObj.AuxDialogs[wobjPsListDlgId] = PsDlgContext.hwndDlg;

    PsDlgContext.ListView = GetDlgItem(PsDlgContext.hwndDlg, IDC_PSLIST_LISTVIEW);
    PsDlgContext.StatusBar = GetDlgItem(PsDlgContext.hwndDlg, IDC_PSLIST_STATUSBAR);
    PsDlgContext.TreeList = GetDlgItem(PsDlgContext.hwndDlg, IDC_PSLIST_TREELIST);

    SendMessage(PsDlgContext.StatusBar, SB_SETPARTS, 3, (LPARAM)&SbParts);

    g_CurrentProcessEntry = NULL;

    if (PsDlgContext.ListView) {
        ListView_SetImageList(PsDlgContext.ListView, g_ListViewImages, LVSIL_SMALL);
        ListView_SetExtendedListViewStyle(PsDlgContext.ListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER);
        SetWindowTheme(PsDlgContext.ListView, TEXT("Explorer"), NULL);

        RtlSecureZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT | LVCF_WIDTH | LVCF_ORDER | LVCF_IMAGE;
        col.iSubItem++;
        col.pszText = TEXT("TID");
        col.fmt = LVCFMT_CENTER | LVCFMT_BITMAP_ON_RIGHT;
        col.iImage = ImageList_GetImageCount(g_ListViewImages) - 1;
        col.cx = 60;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        col.iImage = I_IMAGENONE;
        col.fmt = LVCFMT_LEFT | LVCFMT_BITMAP_ON_RIGHT;

        col.iSubItem++;
        col.pszText = TEXT("Priority");
        col.iOrder++;
        col.cx = 100;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        col.iSubItem++;
        col.pszText = TEXT("State");
        col.iOrder++;
        col.cx = 150;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        col.iSubItem++;
        col.pszText = TEXT("Object");
        col.iOrder++;
        col.cx = 150;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        col.iSubItem++;
        col.pszText = TEXT("StartAddress");
        col.iOrder++;
        col.cx = 140;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        col.iSubItem++;
        col.pszText = TEXT("Module (System threads)");
        col.iOrder++;
        col.cx = 200;
        ListView_InsertColumn(PsDlgContext.ListView, col.iSubItem, &col);

        PsDlgContext.lvColumnCount = col.iSubItem;
    }

    if (PsDlgContext.TreeList) {
        RtlSecureZeroMemory(&hdritem, sizeof(hdritem));
        hdritem.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
        hdritem.fmt = HDF_LEFT | HDF_BITMAP_ON_RIGHT | HDF_STRING;
        hdritem.cxy = 300;
        hdritem.pszText = TEXT("Process");
        TreeList_InsertHeaderItem(PsDlgContext.TreeList, 0, &hdritem);

        hdritem.cxy = 130;
        hdritem.pszText = TEXT("Object");
        TreeList_InsertHeaderItem(PsDlgContext.TreeList, 1, &hdritem);

        hdritem.cxy = 180;
        hdritem.pszText = TEXT("User");
        TreeList_InsertHeaderItem(PsDlgContext.TreeList, 2, &hdritem);
    }

    PsListDialogResize();

    CreateProcessTreeList();
}

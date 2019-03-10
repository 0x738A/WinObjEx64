/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2018 - 2019
*
*  TITLE:       WINE.H
*
*  VERSION:     1.73
*
*  DATE:        09 Mar 2019
*
*  Agent Donald code.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#pragma once

#define OBJECT_TYPES_FIRST_ENTRY_WINE(ObjectTypes) (POBJECT_TYPE_INFORMATION) \
    RtlOffsetToPointer(ObjectTypes, ALIGN_UP(sizeof(OBJECT_TYPES_INFORMATION), ULONG))

typedef char* (__cdecl *pwine_get_version)(void);

static const char *wine_get_version(void)
{
    pwine_get_version pfn;
    HMODULE hmod;

    hmod = GetModuleHandle(TEXT("ntdll.dll"));
    if (hmod) {
        pfn = (pwine_get_version)GetProcAddress(hmod, "wine_get_version");
        if (pfn)
            return pfn();
    }
    return NULL;
}

static int is_wine(void)
{
    pwine_get_version pfn;
    HMODULE hmod;

    hmod = GetModuleHandle(TEXT("ntdll.dll"));
    if (hmod) {
        pfn = (pwine_get_version)GetProcAddress(hmod, "wine_get_version");
        if (pfn)
            return 1;
    }
    return 0;
}

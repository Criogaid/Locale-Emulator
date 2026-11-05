#include "../../Native/LocaleEmulator/LocaleEmulator.h"
#include <cstdio>

int wmain()
{
    using CreateDirectory = NTSTATUS (NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
    auto createDirectory = reinterpret_cast<CreateDirectory>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateDirectoryObject"));
    if (!createDirectory) return 1;
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, nullptr, 0, nullptr, nullptr);
    HANDLE root = nullptr, directories[64] = {}, section = nullptr, found = nullptr;
    NTSTATUS status = createDirectory(&root, DIRECTORY_ALL_ACCESS, &attributes);
    if (NT_FAILED(status)) { printf("directory root status=%08lx\n", status); return 1; }
    WCHAR nameBuffer[128];
    UNICODE_STRING name;
    for (ULONG i = 0; i < countof(directories); ++i)
    {
        swprintf_s(nameBuffer, L"Directory-%02lu-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz", i);
        RtlInitUnicodeString(&name, nameBuffer);
        InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, root, nullptr);
        status = createDirectory(&directories[i], DIRECTORY_ALL_ACCESS, &attributes);
        if (NT_FAILED(status)) { printf("directory create status=%08lx\n", status); return 1; }
    }
    RtlInitUnicodeString(&name, L"NativeContractSection");
    InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, directories[63], nullptr);
    LARGE_INTEGER size; size.QuadPart = 4096;
    status = NtCreateSection(&section, SECTION_ALL_ACCESS, &attributes, &size, PAGE_READWRITE, SEC_COMMIT, nullptr);
    bool passed = NT_SUCCESS(status) && LeFindSectionObject(&found, &name, 1, root);
    printf("controlled directory entries=64 (>1024 bytes), found=%d status=%08lx\n", passed, status);
    if (found) NtClose(found);
    if (section) NtClose(section);
    for (HANDLE directory : directories) NtClose(directory);
    NtClose(root);
    return passed ? 0 : 1;
}

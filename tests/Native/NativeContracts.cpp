#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cwchar>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <cstdlib>

// Wire layout of the existing public loader ABI; no production-only exports are needed.
struct TimeFields { SHORT Year, Month, Day, Hour, Minute, Second, Milliseconds, Weekday; };
struct TimeZone { LONG Bias; WCHAR StandardName[32]; TimeFields StandardStart; LONG StandardBias; WCHAR DaylightName[32]; TimeFields DaylightStart; LONG DaylightBias; };
struct Environment { ULONG Ansi, Oem, Locale, Charset, HookUI; WCHAR Face[32]; TimeZone Zone; ULONG64 Count; };
struct String64 { USHORT Length, MaximumLength; ULONG64 Offset; };
struct RegistryEntry { ULONG64 Root; String64 SubKey, ValueName; ULONG Type; ULONG64 Data, DataSize; };
struct Redirect { RegistryEntry Original, Redirected; };
struct ProcessInfo : PROCESS_INFORMATION { void* FirstCall; };
struct FullInfo { ULONG TitleIndex, Type, DataOffset, DataLength, NameLength; WCHAR Name[1]; };
static_assert(sizeof(Environment) == 264 && sizeof(Redirect) == 128, "loader ABI");
using Create = LONG(WINAPI*)(Environment*, PCWSTR, PWSTR, PCWSTR, ULONG, STARTUPINFOW*, ProcessInfo*, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, void*, HANDLE);
using Query = LONG(NTAPI*)(HANDLE, UNICODE_STRING*, ULONG, void*, ULONG, ULONG*);
static int failures;
static void Check(bool condition, const char* name) { if (!condition) { printf("FAIL %s error=%lu\n", name, GetLastError()); ++failures; } }
static WCHAR executable[32768], directory[32768];

static int RegistryContract(bool includeOem)
{
    Check(GetACP() == 932, "injected child ACP");
    HKEY original = nullptr, reference = nullptr;
    Check(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage", 0, KEY_QUERY_VALUE, &original) == ERROR_SUCCESS, "open codepage key");
    WCHAR keyName[128]; swprintf_s(keyName, L"Software\\LocaleEmulatorNativeContracts-%lu", GetCurrentProcessId());
    Check(RegCreateKeyExW(HKEY_CURRENT_USER, keyName, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &reference, nullptr) == ERROR_SUCCESS, "create reference");
    const WCHAR data[] = L"932";
    auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryValueKey"));
    WCHAR acp[] = L"ACP", oem[] = L"OEMCP";
    PWSTR names[] = {acp, oem};
    for (ULONG index = 0; index < (includeOem ? 2ul : 1ul); ++index)
    {
        Check(RegSetValueExW(reference, names[index], 0, REG_SZ, reinterpret_cast<const BYTE*>(data), sizeof(data)) == ERROR_SUCCESS, "set reference");
        // Both fixture names fit in UNICODE_STRING without narrowing their range.
        const USHORT nameBytes = static_cast<USHORT>(wcslen(names[index]) * sizeof(WCHAR));
        UNICODE_STRING value = {nameBytes, static_cast<USHORT>(nameBytes + sizeof(WCHAR)), names[index]};
        for (ULONG kind : {1ul, 3ul})
        {
            for (ULONG length = 0; length <= 48; ++length)
            {
                alignas(8) BYTE actual[80], expected[80]; memset(actual, 0xA5, sizeof(actual)); memset(expected, 0xA5, sizeof(expected));
                ULONG actualSize = 0, expectedSize = 0;
                LONG actualStatus = query(original, &value, kind, actual + 8, length, &actualSize);
                LONG expectedStatus = query(reference, &value, kind, expected + 8, length, &expectedSize);
                if (actualStatus != expectedStatus || actualSize != expectedSize)
                    printf("registry name=%ls kind=%lu length=%lu status=%08lx/%08lx size=%lu/%lu\n", names[index], kind, length, actualStatus, expectedStatus, actualSize, expectedSize);
                Check(actualStatus == expectedStatus && actualSize == expectedSize, "registry size/status matches OS");
                for (ULONG byte = 0; byte < sizeof(actual); ++byte)
                    if (byte < 8 || byte >= 8 + length) Check(actual[byte] == 0xA5, "registry canary");
                const ULONG headerBytes = offsetof(FullInfo, Name);
                if (length >= headerBytes)
                {
                    auto a = reinterpret_cast<const FullInfo*>(actual + 8), e = reinterpret_cast<const FullInfo*>(expected + 8);
                    Check(memcmp(a, e, headerBytes) == 0, "registry header matches OS");
                    // STATUS_BUFFER_OVERFLOW does not specify partial-WCHAR contents.
                    if (actualStatus >= 0)
                    {
                        Check(memcmp(a->Name, e->Name, e->NameLength) == 0, "registry name contents match OS");
                        Check(memcmp(actual + 8 + a->DataOffset, data, sizeof(data)) == 0, "registry data");
                    }
                }
            }
            ULONG a = 0, e = 0;
            Check(query(original, &value, kind, nullptr, 0, &a) == query(reference, &value, kind, nullptr, 0, &e) && a == e, "null sizing query");
            alignas(8) BYTE misalignedActual[64] = {}, misalignedExpected[64] = {};
            a = e = 0;
            LONG actualStatus = query(original, &value, kind, misalignedActual + 1, 40, &a);
            LONG expectedStatus = query(reference, &value, kind, misalignedExpected + 1, 40, &e);
            Check(actualStatus == expectedStatus && a == e, "misaligned query matches OS");
        }
        printf("registry name=%ls standard+Align64 sizes=0..48 names+canaries checked\n", names[index]);
    }
    RegCloseKey(original); RegCloseKey(reference); RegDeleteKeyW(HKEY_CURRENT_USER, keyName);
    printf("registry failures=%d\n", failures);
    return failures;
}

static bool HasChild()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    Check(snapshot != INVALID_HANDLE_VALUE, "process snapshot");
    PROCESSENTRY32W entry = {}; entry.dwSize = sizeof(entry); bool found = false;
    if (Process32FirstW(snapshot, &entry)) do {
        if (entry.th32ParentProcessID == GetCurrentProcessId()) found = true;
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot); return found;
}

static int FailureContract()
{
    Check(GetACP() == 932, "failure parent initialized");
    WCHAR dll[32768], held[32768], command[32768];
    swprintf_s(dll, L"%s\\LocaleEmulator.dll", directory); swprintf_s(held, L"%s.held", dll);
    Check(MoveFileW(dll, held) != FALSE, "isolate missing DLL");
    swprintf_s(command, L"\"%s\" --failed-child", executable);
    STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION info = {};
    SetLastError(0); BOOL created = CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, directory, &startup, &info);
    DWORD error = GetLastError();
    BOOL wow64 = FALSE;
    Check(IsWow64Process(GetCurrentProcess(), &wow64) != FALSE, "query parent architecture");
    if (wow64)
    {
        WCHAR windows[32768], crossCommand[32768]; GetWindowsDirectoryW(windows, _countof(windows));
        swprintf_s(crossCommand, L"\"%s\\Sysnative\\cmd.exe\" /c exit 0", windows);
        PROCESS_INFORMATION crossInfo = {};
        BOOL crossCreated = CreateProcessW(nullptr, crossCommand, nullptr, nullptr, FALSE, 0, nullptr, directory, &startup, &crossInfo);
        Check(crossCreated != FALSE, "cross-bitness deliberate non-injection");
        if (crossCreated)
        {
            DWORD wait = WaitForSingleObject(crossInfo.hProcess, 5000), code = 0;
            if (wait != WAIT_OBJECT_0) { TerminateProcess(crossInfo.hProcess, 99); WaitForSingleObject(crossInfo.hProcess, 5000); }
            GetExitCodeProcess(crossInfo.hProcess, &code); Check(wait == WAIT_OBJECT_0 && code == 0, "cross-bitness child completes");
            CloseHandle(crossInfo.hThread); CloseHandle(crossInfo.hProcess);
        }
        printf("cross-bitness missing-dll created=%d\n", crossCreated);
    }
    Check(MoveFileW(held, dll) != FALSE, "restore DLL");
    Check(!created && error != ERROR_SUCCESS, "missing DLL returns failure");
    if (created) { TerminateProcess(info.hProcess, 99); WaitForSingleObject(info.hProcess, 5000); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
    Check(!info.hProcess && !info.hThread, "failed creation returns no handles");
    Check(!HasChild(), "no surviving failed child");
    printf("missing-dll created=%d error=%lu failures=%d\n", created, error, failures);
    return failures;
}

static void LargeAttributeListContract()
{
    using NativeCreate = LONG(NTAPI*)(HANDLE*, HANDLE*, ACCESS_MASK, ACCESS_MASK, void*, void*, ULONG, ULONG,
        void*, void*, void*);
    auto create = reinterpret_cast<NativeCreate>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateUserProcess"));
    Check(create != nullptr, "large attribute list syscall");
    if (create == nullptr) return;

    constexpr SIZE_T AttributeBytes = 1024 * 1024;
    auto list = static_cast<BYTE*>(VirtualAlloc(nullptr, AttributeBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(list != nullptr, "large attribute list allocation");
    if (list == nullptr) return;
    ZeroMemory(list, AttributeBytes);
    *reinterpret_cast<ULONG_PTR*>(list) = AttributeBytes;

    HANDLE process = nullptr, thread = nullptr;
    LONG status = create(&process, &thread, 0, 0, nullptr, nullptr, 0, 0, nullptr, nullptr, list);
    printf("large-attribute-list bytes=%zu status=%08lx\n", AttributeBytes, status);
    Check(status < 0, "large attribute list rejected without crash");
    if (process != nullptr)
    {
        TerminateProcess(process, 99);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }
    if (thread != nullptr) CloseHandle(thread);
    VirtualFree(list, 0, MEM_RELEASE);
}


static void FaultContracts();
static void RightsContracts(bool injected);
static void CleanupOwnChildren();
static int GuardContract(ULONG count, bool overflow)
{
    WCHAR loader[32768]; swprintf_s(loader, L"%s\\LoaderDll.dll", directory);
    HMODULE module = LoadLibraryW(loader); Check(module != nullptr, "load loader");
    auto create = reinterpret_cast<Create>(GetProcAddress(module, "LeCreateProcess")); Check(create != nullptr, "loader export");
    if (!create) return failures;
    SYSTEM_INFO system; GetSystemInfo(&system);
    auto memory = static_cast<BYTE*>(VirtualAlloc(nullptr, system.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous; Check(memory && VirtualProtect(memory + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &previous), "guard page");
    const WCHAR path[] = L"System\\CurrentControlSet\\Control\\Nls\\CodePage", name[] = L"ACP", data[] = L"932";
    const WCHAR oemName[] = L"OEMCP";
    size_t bytes = sizeof(Environment) + count * sizeof(Redirect) + (count ? sizeof(path) + sizeof(name) + sizeof(data) : 0) + (count > 1 ? sizeof(oemName) : 0);
    auto env = reinterpret_cast<Environment*>(memory + system.dwPageSize - bytes); ZeroMemory(env, bytes);
    env->Ansi = env->Oem = 932; env->Locale = 0x411; env->Charset = 128; env->Count = overflow && count == 0 ? UINT64_MAX : count;
    auto entries = reinterpret_cast<Redirect*>(env + 1); auto buffer = reinterpret_cast<BYTE*>(entries + count);
    if (count)
    {
        RegistryEntry original = {}; original.Root = static_cast<ULONG64>(static_cast<LONG64>(reinterpret_cast<LONG_PTR>(HKEY_LOCAL_MACHINE)));
        original.SubKey = {sizeof(path) - 2, sizeof(path), static_cast<ULONG64>(buffer - reinterpret_cast<BYTE*>(env))}; memcpy(buffer, path, sizeof(path)); buffer += sizeof(path);
        original.ValueName = {sizeof(name) - 2, sizeof(name), static_cast<ULONG64>(buffer - reinterpret_cast<BYTE*>(env))}; memcpy(buffer, name, sizeof(name)); buffer += sizeof(name);
        for (ULONG i = 0; i < count; ++i) { entries[i].Original = entries[i].Redirected = original; entries[i].Redirected.Type = REG_SZ; entries[i].Redirected.Data = static_cast<ULONG64>(buffer - reinterpret_cast<BYTE*>(env)); entries[i].Redirected.DataSize = sizeof(data); }
        memcpy(buffer, data, sizeof(data));
        if (count > 1)
        {
            buffer += sizeof(data);
            entries[1].Original.ValueName = {sizeof(oemName) - sizeof(WCHAR), sizeof(oemName), static_cast<ULONG64>(buffer - reinterpret_cast<BYTE*>(env))};
            entries[1].Redirected.ValueName = entries[1].Original.ValueName;
            memcpy(buffer, oemName, sizeof(oemName));
        }
        if (overflow) entries[0].Redirected.Data = UINT64_MAX - 1;
    }
    WCHAR command[32768]; swprintf_s(command, L"\"%s\" --registry-and-failure %lu", executable, count);
    STARTUPINFOW startup = {}; startup.cb = sizeof(startup); ProcessInfo info = {};
    const bool CopyOnly = count == 0;
    if (count == 1 && !overflow) RightsContracts(false);
    LONG status = create(env, executable, command, directory, CopyOnly ? CREATE_SUSPENDED : 0, &startup, &info, nullptr, nullptr, nullptr, nullptr);
    printf("export=LeCreateProcess entries=%lu bytes=%zu overflow=%d status=%08lx\n", count, bytes, overflow, status);
    if (overflow) Check(status == static_cast<LONG>(0xC0000095), "overflow rejected");
    else Check(status >= 0, "guarded legal blob accepted");
    if (status >= 0 && CopyOnly) { TerminateProcess(info.hProcess, 0); WaitForSingleObject(info.hProcess, 5000); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
    if (status >= 0 && !CopyOnly) { DWORD wait = WaitForSingleObject(info.hProcess, 20000), code = 0; Check(wait == WAIT_OBJECT_0, "child completes"); if (wait != WAIT_OBJECT_0) TerminateProcess(info.hProcess, 99); GetExitCodeProcess(info.hProcess, &code); printf("child_exit=%08lx\n", code); Check(code == 0, "child contracts"); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
    if (count == 1 && !overflow && GetModuleHandleW(L"LocaleEmulator.dll")) { RightsContracts(true); FaultContracts(); }
    if (count == 1 && !overflow && GetModuleHandleW(L"LocaleEmulator.dll")) { LargeAttributeListContract(); }
    VirtualFree(memory, 0, MEM_RELEASE);
    CleanupOwnChildren();
    return failures;
}

// Only the core image mapped in this test process is intercepted.
using NtWrite = LONG (NTAPI*)(HANDLE, void*, const void*, SIZE_T, SIZE_T*);
using NtProtect = LONG (NTAPI*)(HANDLE, void**, SIZE_T*, ULONG, ULONG*);
using NtFree = LONG (NTAPI*)(HANDLE, void**, SIZE_T*, ULONG);
using NtSection = LONG (NTAPI*)(HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*, LARGE_INTEGER*, ULONG, ULONG, HANDLE);
using NtResume = LONG (NTAPI*)(HANDLE, ULONG*);
using NtTerminate = LONG (NTAPI*)(HANDLE, LONG);
static NtWrite realWrite;
static NtProtect realProtect;
static NtFree realFree;
static NtSection realSection;
static NtResume realResume;
static NtTerminate realTerminate, realTerminateThread;
enum class Fault { Section, Protect, Write, PartialWrite, Resume, TerminateDenied, CleanupDenied };
struct FaultState
{
    Fault Mode;
    bool Active, Fired, Patched, Restored, Freed, Terminated;
    ULONG PatchWrites, RestoreWrites, FreeCalls, ProcessTermDenied, ThreadTermCalls;
    SIZE_T PartialBytes;
    HANDLE Observer;
    DWORD Pid;
    void* Shadow;
    void* Patch;
    BYTE Original[5];
};
static FaultState fault;
static constexpr SIZE_T PartialWriteBytes = 2;
static constexpr LONG AccessDenied = static_cast<LONG>(0xC0000022);
static bool Remote(HANDLE process) { return process != GetCurrentProcess() && GetProcessId(process) != GetCurrentProcessId(); }
static void Capture(HANDLE process)
{
    if (fault.Observer) return;
    fault.Pid = GetProcessId(process);
    Check(DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(), &fault.Observer, 0, FALSE, DUPLICATE_SAME_ACCESS) != FALSE, "retain child observer");
    SIZE_T read = 0;
    Check(ReadProcessMemory(process, fault.Patch, fault.Original, sizeof(fault.Original), &read) && read == sizeof(fault.Original), "capture original remote bytes");
}
static bool OriginalBytes(HANDLE process)
{
    BYTE bytes[5]; SIZE_T read = 0;
    return ReadProcessMemory(process, fault.Patch, bytes, sizeof(bytes), &read) && read == sizeof(bytes) && memcmp(bytes, fault.Original, sizeof(bytes)) == 0;
}
static LONG NTAPI InterceptWrite(HANDLE process, void* address, const void* data, SIZE_T size, SIZE_T* written)
{
    if (fault.Active && Remote(process))
    {
        Capture(process);
        if (size > sizeof(fault.Original)) fault.Shadow = address;
        if (address == fault.Patch && size == sizeof(fault.Original))
        {
            if (!fault.Fired && (fault.Mode == Fault::Write || fault.Mode == Fault::PartialWrite))
            {
                fault.Fired = true;
                if (fault.Mode == Fault::PartialWrite)
                {
                    SIZE_T partial = 0;
                    LONG status = realWrite(process, address, data, PartialWriteBytes, &partial);
                    Check(status >= 0 && partial == PartialWriteBytes, "write real partial prefix");
                    fault.PartialBytes = partial;
                    Check(!OriginalBytes(process), "partial overwrite visible before rollback");
                    if (written) *written = partial;
                    return static_cast<LONG>(0x8000000D); // STATUS_PARTIAL_COPY permits a transferred prefix.
                }
                if (written) *written = 0;
                return AccessDenied;
            }
            LONG status = realWrite(process, address, data, size, written);
            if (status >= 0)
            {
                if (memcmp(data, fault.Original, sizeof(fault.Original)) == 0)
                {
                    ++fault.RestoreWrites;
                    fault.Restored = OriginalBytes(process);
                }
                else
                {
                    ++fault.PatchWrites;
                    fault.Patched = true;
                }
            }
            return status;
        }
    }
    return realWrite(process, address, data, size, written);
}
static LONG NTAPI InterceptProtect(HANDLE process, void** address, SIZE_T* size, ULONG protection, ULONG* previous)
{
    if (fault.Active && Remote(process) && *address == fault.Patch && *size == sizeof(fault.Original))
    {
        Capture(process);
        if (!fault.Fired && fault.Mode == Fault::Protect && protection == PAGE_EXECUTE_READWRITE)
        {
            fault.Fired = true;
            return AccessDenied;
        }
    }
    return realProtect(process, address, size, protection, previous);
}
static LONG NTAPI InterceptFree(HANDLE process, void** address, SIZE_T* size, ULONG flags)
{
    bool shadow = fault.Active && Remote(process) && *address == fault.Shadow;
    void* released = *address;
    LONG status = realFree(process, address, size, flags);
    if (shadow)
    {
        ++fault.FreeCalls;
        MEMORY_BASIC_INFORMATION memory = {};
        fault.Freed = status >= 0 && VirtualQueryEx(process, released, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE;
    }
    return status;
}
static LONG NTAPI InterceptSection(HANDLE* section, ACCESS_MASK access, OBJECT_ATTRIBUTES* attributes, LARGE_INTEGER* size, ULONG protection, ULONG flags, HANDLE file)
{
    if (fault.Active && fault.Patched && !fault.Fired && (fault.Mode == Fault::Section || fault.Mode == Fault::TerminateDenied || fault.Mode == Fault::CleanupDenied))
    {
        fault.Fired = true;
        return AccessDenied;
    }
    return realSection(section, access, attributes, size, protection, flags, file);
}
static LONG NTAPI InterceptResume(HANDLE thread, ULONG* previous)
{
    if (fault.Active && fault.Patched && !fault.Fired && fault.Mode == Fault::Resume)
    {
        fault.Fired = true;
        return AccessDenied;
    }
    return realResume(thread, previous);
}
static LONG NTAPI InterceptTerminate(HANDLE process, LONG status)
{
    if (fault.Active && GetProcessId(process) == fault.Pid)
    {
        if (fault.Mode != Fault::Resume)
            Check(fault.Restored && OriginalBytes(process) && fault.Freed, "rollback completed before termination");
        if (fault.Mode == Fault::TerminateDenied || fault.Mode == Fault::CleanupDenied)
        {
            ++fault.ProcessTermDenied;
            return AccessDenied;
        }
        fault.Terminated = true;
    }
    return realTerminate(process, status);
}

static LONG NTAPI InterceptTerminateThread(HANDLE thread, LONG status)
{
    if (fault.Active && GetProcessIdOfThread(thread) == fault.Pid &&
        (fault.Mode == Fault::TerminateDenied || fault.Mode == Fault::CleanupDenied))
    {
        ++fault.ThreadTermCalls;
        if (fault.Mode == Fault::CleanupDenied) return AccessDenied;
        LONG result = realTerminateThread(thread, status);
        fault.Terminated = result >= 0;
        return result;
    }
    return realTerminateThread(thread, status);
}

struct ImportPatch
{
    ULONG_PTR* Slot = nullptr;
    ULONG_PTR Original = 0;
    DWORD InitialProtection = 0;
    const char* Symbol = nullptr;
    bool Set(ULONG_PTR value)
    {
        DWORD old = 0, ignored = 0;
        if (!VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &old)) return false;
        *Slot = value;
        return VirtualProtect(Slot, sizeof(*Slot), old, &ignored) != FALSE;
    }
    bool Install(HMODULE module, const char* symbol, ULONG_PTR replacement)
    {
        auto base = reinterpret_cast<BYTE*>(module);
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return false;
        auto headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (headers->Signature != IMAGE_NT_SIGNATURE || headers->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
        const DWORD imageSize = headers->OptionalHeader.SizeOfImage;
        const auto& imports = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (imports.VirtualAddress >= imageSize || imports.Size > imageSize - imports.VirtualAddress) return false;
        auto descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
        for (DWORD i = 0; i < imports.Size / sizeof(*descriptors) && descriptors[i].Name; ++i)
        {
            const auto& descriptor = descriptors[i];
            if (descriptor.Name >= imageSize || strnlen_s(reinterpret_cast<const char*>(base + descriptor.Name), imageSize - descriptor.Name) == imageSize - descriptor.Name) return false;
            if (_stricmp(reinterpret_cast<const char*>(base + descriptor.Name), "ntdll.dll")) continue;
            if (!descriptor.OriginalFirstThunk || descriptor.OriginalFirstThunk >= imageSize || descriptor.FirstThunk >= imageSize) return false;
            const DWORD count = (imageSize - (descriptor.OriginalFirstThunk > descriptor.FirstThunk ? descriptor.OriginalFirstThunk : descriptor.FirstThunk)) / sizeof(IMAGE_THUNK_DATA);
            auto names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + descriptor.OriginalFirstThunk);
            auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor.FirstThunk);
            for (DWORD j = 0; j < count && names[j].u1.AddressOfData; ++j)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal)) continue;
                DWORD rva = names[j].u1.AddressOfData;
                if (rva >= imageSize || imageSize - rva <= offsetof(IMAGE_IMPORT_BY_NAME, Name)) return false;
                auto name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + rva);
                const SIZE_T capacity = imageSize - rva - offsetof(IMAGE_IMPORT_BY_NAME, Name);
                if (strnlen_s(name->Name, capacity) == capacity) return false;
                if (strcmp(name->Name, symbol)) continue;
                Slot = &slots[j].u1.Function;
                Original = *Slot;
                Symbol = symbol;
                MEMORY_BASIC_INFORMATION memory = {};
                if (VirtualQuery(Slot, &memory, sizeof(memory)) != sizeof(memory)) return false;
                InitialProtection = memory.Protect;
                return Set(replacement);
            }
        }
        return false;
    }
    ~ImportPatch()
    {
        if (!Slot) return;
        MEMORY_BASIC_INFORMATION memory = {};
        bool restored = Set(Original) && *Slot == Original && VirtualQuery(Slot, &memory, sizeof(memory)) == sizeof(memory) && memory.Protect == InitialProtection;
        Check(restored, "restore core IAT and page protection");
        printf("iat=%s original+protection-restored=%d\n", Symbol, restored);
    }
};

// The existing native hook boundary uses this Windows x86 wire layout, absent from winternl.h.
struct NativeCreateInfo
{
    SIZE_T Size;
    ULONG State;
    struct
    {
        ULONG Flags;
        HANDLE File, Section;
        ULONG64 ParametersNative;
        ULONG ParametersWow64, ParameterFlags;
        ULONG64 PebNative;
        ULONG PebWow64;
        ULONG64 Manifest;
        ULONG ManifestSize;
    } Success;
};
static_assert(sizeof(NativeCreateInfo) == 0x48, "x86 PS_CREATE_INFO wire layout");
struct NativeAttribute { ULONG_PTR Number; SIZE_T Size; void* Value; SIZE_T* Returned; };
struct NativeAttributes { SIZE_T Size; NativeAttribute Image, Client; };
struct NativeRequest
{
    ACCESS_MASK ProcessAccess = PROCESS_ALL_ACCESS;
    ACCESS_MASK ThreadAccess = THREAD_ALL_ACCESS;
    ULONG ThreadFlags = 0;
    ULONG HandleAttributes = 0;
};
static LONG CreateNativeChild(PROCESS_INFORMATION& info, const NativeRequest& request = {}, DWORD* pid = nullptr)
{
    using MakeParameters = LONG (NTAPI*)(RTL_USER_PROCESS_PARAMETERS**, UNICODE_STRING*, UNICODE_STRING*, UNICODE_STRING*, UNICODE_STRING*, void*, UNICODE_STRING*, UNICODE_STRING*, UNICODE_STRING*, UNICODE_STRING*, ULONG);
    using DestroyParameters = void (NTAPI*)(RTL_USER_PROCESS_PARAMETERS*);
    using NativeCreate = LONG (NTAPI*)(HANDLE*, HANDLE*, ACCESS_MASK, ACCESS_MASK, OBJECT_ATTRIBUTES*, OBJECT_ATTRIBUTES*, ULONG, ULONG, RTL_USER_PROCESS_PARAMETERS*, NativeCreateInfo*, NativeAttributes*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto make = reinterpret_cast<MakeParameters>(GetProcAddress(ntdll, "RtlCreateProcessParametersEx"));
    auto destroy = reinterpret_cast<DestroyParameters>(GetProcAddress(ntdll, "RtlDestroyProcessParameters"));
    auto create = reinterpret_cast<NativeCreate>(GetProcAddress(ntdll, "NtCreateUserProcess"));
    if (!make || !destroy || !create) return static_cast<LONG>(0xC000007A);
    WCHAR image[32768], command[32768];
    swprintf_s(image, L"\\??\\%s", executable);
    swprintf_s(command, L"\"%s\" --failed-child", executable);
    UNICODE_STRING imageName = {static_cast<USHORT>(wcslen(image) * sizeof(WCHAR)), 0, image};
    UNICODE_STRING commandLine = {static_cast<USHORT>(wcslen(command) * sizeof(WCHAR)), 0, command};
    imageName.MaximumLength = imageName.Length + sizeof(WCHAR);
    commandLine.MaximumLength = commandLine.Length + sizeof(WCHAR);
    RTL_USER_PROCESS_PARAMETERS* parameters = nullptr;
    LONG status = make(&parameters, &imageName, nullptr, nullptr, &commandLine, nullptr, nullptr, nullptr, nullptr, nullptr, 1);
    if (status < 0) return status;
    NativeCreateInfo creation = {}; creation.Size = sizeof(creation);
    constexpr ULONG_PTR ImageNameAttribute = 0x20005;
    struct { HANDLE Process, Thread; } client = {};
    constexpr ULONG_PTR ClientIdAttribute = 0x10003;
    NativeAttributes attributes = {sizeof(attributes), {ImageNameAttribute, imageName.Length, image, nullptr}, {ClientIdAttribute, sizeof(client), &client, nullptr}};
    OBJECT_ATTRIBUTES objectAttributes = {};
    objectAttributes.Length = sizeof(objectAttributes);
    objectAttributes.Attributes = request.HandleAttributes;
    status = create(&info.hProcess, &info.hThread, request.ProcessAccess, request.ThreadAccess, &objectAttributes, &objectAttributes, 0, request.ThreadFlags, parameters, &creation, &attributes);
    if (pid) *pid = HandleToULong(client.Process);
    if (creation.State == 6) { if (creation.Success.File) CloseHandle(creation.Success.File); if (creation.Success.Section) CloseHandle(creation.Success.Section); }
    else if (creation.State == 2 && creation.Success.Flags) CloseHandle(reinterpret_cast<HANDLE>(creation.Success.Flags));
    destroy(parameters);
    return status;
}
struct ChildCleanup
{
    PROCESS_INFORMATION& Info;
    HANDLE Observer = nullptr;
    DWORD Pid;
    ChildCleanup(PROCESS_INFORMATION& info, DWORD pid) : Info(info), Pid(pid)
    {
        if (pid) Observer = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        Check(Observer != nullptr || !pid || GetLastError() == ERROR_INVALID_PARAMETER, "open independent child cleanup handle");
    }
    DWORD Wait() const { return Observer ? WaitForSingleObject(Observer, 0) : WAIT_OBJECT_0; }
    ~ChildCleanup()
    {
        if (Observer && Wait() == WAIT_TIMEOUT)
        {
            bool own = false;
            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            PROCESSENTRY32W entry = {}; entry.dwSize = sizeof(entry);
            if (snapshot != INVALID_HANDLE_VALUE)
            {
                if (Process32FirstW(snapshot, &entry)) do {
                    if (entry.th32ProcessID == Pid) own = entry.th32ParentProcessID == GetCurrentProcessId();
                } while (Process32NextW(snapshot, &entry));
                CloseHandle(snapshot);
            }
            Check(own, "cleanup PID belongs to this test");
            if (own)
            {
                bool stopped = TerminateProcess(Observer, 99) && WaitForSingleObject(Observer, 5000) == WAIT_OBJECT_0;
                Check(stopped, "finally terminate own test child");
                printf("cleanup own-pid=%lu stopped=%d\n", Pid, stopped);
            }
        }
        for (HANDLE handle : {Info.hThread, Info.hProcess})
        {
            if (!handle) continue;
            Check(SetHandleInformation(handle, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0) != FALSE, "clear test output handle close protection");
            Check(CloseHandle(handle) != FALSE, "close test output handle");
        }
        if (Observer) CloseHandle(Observer);
    }
};
static void CleanupOwnChildren()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    Check(snapshot != INVALID_HANDLE_VALUE, "final child cleanup snapshot");
    if (snapshot == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W entry = {}; entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) do
    {
        if (entry.th32ParentProcessID != GetCurrentProcessId()) continue;
        PROCESS_INFORMATION info = {};
        ChildCleanup child(info, entry.th32ProcessID);
        Check(child.Wait() != WAIT_TIMEOUT, "no child survives failed launcher return");
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
}
struct HandleSnapshot { ACCESS_MASK Access; DWORD Flags; };
static HandleSnapshot InspectHandle(HANDLE handle)
{
    using QueryObject = LONG (NTAPI*)(HANDLE, OBJECT_INFORMATION_CLASS, void*, ULONG, ULONG*);
    auto query = reinterpret_cast<QueryObject>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject"));
    PUBLIC_OBJECT_BASIC_INFORMATION basic = {};
    DWORD flags = 0;
    Check(query && query(handle, ObjectBasicInformation, &basic, sizeof(basic), nullptr) >= 0, "query SDK granted access");
    Check(GetHandleInformation(handle, &flags) != FALSE, "query SDK handle attributes");
    return {basic.GrantedAccess, flags};
}
static void RightsContracts(bool injected)
{
    struct Case { const char* Name; NativeRequest Request; };
    constexpr ULONG CreateSuspended = 1;
    const Case cases[] = {
        {"no-process-terminate", {PROCESS_ALL_ACCESS & ~PROCESS_TERMINATE, THREAD_ALL_ACCESS, CreateSuspended, 0}},
        {"query-only-inherit", {PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, CreateSuspended, 2}},
        {"limited-thread-resume", {PROCESS_ALL_ACCESS & ~PROCESS_TERMINATE, THREAD_QUERY_INFORMATION | SYNCHRONIZE, 0, 2}},
        {"generic-read", {GENERIC_READ | SYNCHRONIZE, GENERIC_READ | SYNCHRONIZE, CreateSuspended, 2}},
        {"maximum-allowed", {MAXIMUM_ALLOWED, MAXIMUM_ALLOWED, CreateSuspended, 0}}
    };
    static HandleSnapshot expectedProcess[_countof(cases)] = {}, expectedThread[_countof(cases)] = {};
    for (SIZE_T i = 0; i < _countof(cases); ++i)
    {
        PROCESS_INFORMATION info = {}; DWORD pid = 0;
        NativeRequest request = cases[i].Request;
        if (!injected) request.ThreadFlags = CreateSuspended;
        LONG status = CreateNativeChild(info, request, &pid);
        ChildCleanup child(info, pid);
        Check(status >= 0 && info.hProcess && info.hThread, "limited access native creation succeeds");
        if (status >= 0 && info.hProcess && info.hThread)
        {
            HandleSnapshot process = InspectHandle(info.hProcess), thread = InspectHandle(info.hThread);
            if (!injected) { expectedProcess[i] = process; expectedThread[i] = thread; }
            else
            {
                Check(process.Access == expectedProcess[i].Access && thread.Access == expectedThread[i].Access, "outward access exactly matches native baseline");
                Check(process.Flags == expectedProcess[i].Flags && thread.Flags == expectedThread[i].Flags, "outward attributes exactly match native baseline");
            }
            if ((process.Access & PROCESS_TERMINATE) == 0)
            {
                SetLastError(0);
                BOOL terminated = TerminateProcess(info.hProcess, 99);
                Check(!terminated && GetLastError() == ERROR_ACCESS_DENIED, "SDK denies terminate through restricted outward process handle");
            }
            if ((process.Access & PROCESS_VM_OPERATION) == 0)
            {
                SetLastError(0);
                void* memory = VirtualAllocEx(info.hProcess, nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                Check(!memory && GetLastError() == ERROR_ACCESS_DENIED, "SDK denies VM operation through restricted outward process handle");
                if (memory) VirtualFreeEx(info.hProcess, memory, 0, MEM_RELEASE);
            }
            if ((thread.Access & THREAD_SUSPEND_RESUME) == 0)
            {
                SetLastError(0);
                DWORD resumed = ResumeThread(info.hThread);
                Check(resumed == static_cast<DWORD>(-1) && GetLastError() == ERROR_ACCESS_DENIED, "SDK denies resume through restricted outward thread handle");
            }
            printf("rights=%s phase=%s status=%08lx process=%08lx thread=%08lx flags=%lu/%lu\n",
                cases[i].Name, injected ? "injected" : "native", status, process.Access, thread.Access, process.Flags, thread.Flags);
        }
        else printf("rights=%s phase=%s status=%08lx pid=%lu wait=%lu\n", cases[i].Name, injected ? "injected" : "native", status, pid, child.Wait());
    }
    if (!injected) return;
    WCHAR dll[32768], held[32768];
    swprintf_s(dll, L"%s\\LocaleEmulator.dll", directory);
    swprintf_s(held, L"%s.rights-held", dll);
    struct RestoreDll
    {
        PCWSTR Original, Held;
        bool Moved;
        ~RestoreDll() { if (Moved) Check(MoveFileW(Held, Original) != FALSE, "finally restore rights fixture DLL"); }
    } restore{dll, held, MoveFileW(dll, held) != FALSE};
    Check(restore.Moved, "isolate missing DLL for restricted process");
    if (!restore.Moved) return;
    PROCESS_INFORMATION info = {}; DWORD pid = 0;
    NativeRequest request; request.ProcessAccess &= ~PROCESS_TERMINATE;
    LONG status = CreateNativeChild(info, request, &pid);
    ChildCleanup child(info, pid);
    DWORD wait = child.Observer ? WaitForSingleObject(child.Observer, 1000) : WAIT_OBJECT_0;
    printf("rights=missing-dll-no-terminate status=%08lx process=%p thread=%p pid=%lu wait=%lu\n", status, info.hProcess, info.hThread, pid, wait);
    Check(status < 0 && !info.hProcess && !info.hThread, "restricted missing DLL returns failure and zero handles");
    Check(wait == WAIT_OBJECT_0, "restricted missing DLL does not orphan suspended child");
}
static void FaultContracts()
{
    HMODULE core = GetModuleHandleW(L"LocaleEmulator.dll");
    Check(core != nullptr, "fault parent core initialized");
    if (!core) return;
    ImportPatch writes, protects, frees, sections, resumes, terminates, terminateThreads;
    if (!writes.Install(core, "NtWriteVirtualMemory", reinterpret_cast<ULONG_PTR>(InterceptWrite)) ||
        !protects.Install(core, "NtProtectVirtualMemory", reinterpret_cast<ULONG_PTR>(InterceptProtect)) ||
        !frees.Install(core, "NtFreeVirtualMemory", reinterpret_cast<ULONG_PTR>(InterceptFree)) ||
        !sections.Install(core, "NtCreateSection", reinterpret_cast<ULONG_PTR>(InterceptSection)) ||
        !resumes.Install(core, "NtResumeThread", reinterpret_cast<ULONG_PTR>(InterceptResume)) ||
        !terminates.Install(core, "NtTerminateProcess", reinterpret_cast<ULONG_PTR>(InterceptTerminate)) ||
        !terminateThreads.Install(core, "NtTerminateThread", reinterpret_cast<ULONG_PTR>(InterceptTerminateThread)))
    { Check(false, "install bounded core IAT instrumentation"); return; }
    realWrite = reinterpret_cast<NtWrite>(writes.Original);
    realProtect = reinterpret_cast<NtProtect>(protects.Original);
    realFree = reinterpret_cast<NtFree>(frees.Original);
    realSection = reinterpret_cast<NtSection>(sections.Original);
    realResume = reinterpret_cast<NtResume>(resumes.Original);
    realTerminate = reinterpret_cast<NtTerminate>(terminates.Original);
    realTerminateThread = reinterpret_cast<NtTerminate>(terminateThreads.Original);
    const char* const names[] = {"section-after-patch", "remote-protect", "remote-write", "remote-partial-write", "resume-thread", "process-terminate-denied", "both-cleanup-calls-denied"};
    for (Fault mode : {Fault::Section, Fault::Protect, Fault::Write, Fault::PartialWrite, Fault::Resume, Fault::TerminateDenied, Fault::CleanupDenied})
    {
        fault = {}; fault.Mode = mode;
        fault.Patch = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll"));
        WCHAR command[32768]; swprintf_s(command, L"\"%s\" --failed-child", executable);
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION info = {};
        fault.Active = true;
        NativeRequest request;
        if (mode == Fault::CleanupDenied)
        {
            request.ProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
            request.ThreadAccess = THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
            request.HandleAttributes = 2;
        }
        const bool nativeCall = mode == Fault::Resume || mode == Fault::CleanupDenied;
        LONG status = nativeCall ? CreateNativeChild(info, request) :
            (CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, directory, &startup, &info) ? 0 : -1);
        const DWORD error = nativeCall ? 0 : GetLastError();
        fault.Active = false;
        Check(fault.Fired, "requested fault reached");
        ChildCleanup child(info, fault.Pid ? fault.Pid : (info.hProcess ? GetProcessId(info.hProcess) : 0));
        const bool retained = mode == Fault::CleanupDenied && info.hProcess && info.hThread;
        if (mode == Fault::CleanupDenied)
        {
            Check(status < 0 && retained && fault.Observer && WaitForSingleObject(fault.Observer, 0) == WAIT_TIMEOUT, "failed termination retains live cleanup handles");
            if (retained)
            {
                Check((InspectHandle(info.hProcess).Access & PROCESS_TERMINATE) != 0 &&
                    (InspectHandle(info.hThread).Access & THREAD_TERMINATE) != 0, "retained cleanup rights remain usable");
                Check(InspectHandle(info.hProcess).Flags == 0 && InspectHandle(info.hThread).Flags == 0, "private cleanup capabilities are not inheritable");
                Check(TerminateProcess(info.hProcess, 99) && WaitForSingleObject(fault.Observer, 5000) == WAIT_OBJECT_0, "rescue own child through returned cleanup handle");
            }
        }
        else
        {
            Check(status < 0 && !info.hProcess && !info.hThread, "fault returns failure with zero handles");
            Check(fault.Terminated && fault.Observer && WaitForSingleObject(fault.Observer, 5000) == WAIT_OBJECT_0, "fault terminates created child");
        }
        if (mode == Fault::TerminateDenied || mode == Fault::CleanupDenied)
            Check(fault.ProcessTermDenied == 1 && fault.ThreadTermCalls == 1, "failed process termination is checked and thread fallback attempted");
        if (mode != Fault::Resume)
            Check(fault.RestoreWrites == 1 && fault.Restored && fault.FreeCalls == 1 && fault.Freed, "one rollback restores bytes and releases shadow");
        else Check(fault.Patched && fault.Terminated, "resume failure terminates injected suspended child");
        if (fault.Observer) CloseHandle(fault.Observer);
        Check(!HasChild(), "fault leaves no live child");
        printf("fault=%s fired=%d patch=%lu partial=%zu restore=%lu original=%d free=%lu released=%d terminated=%d status=%08lx win32_error=%lu process-term-denied=%lu thread-term-attempts=%lu cleanup-returned=%d\n",
            names[static_cast<unsigned>(mode)], fault.Fired, fault.PatchWrites, fault.PartialBytes, fault.RestoreWrites, fault.Restored, fault.FreeCalls, fault.Freed, fault.Terminated, status, error, fault.ProcessTermDenied, fault.ThreadTermCalls, retained);
    }
}
int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    GetModuleFileNameW(nullptr, executable, _countof(executable)); wcscpy_s(directory, executable); *wcsrchr(directory, L'\\') = 0;
    if (argc > 1 && !wcscmp(argv[1], L"--failed-child")) { Sleep(60000); return 99; }
    if (argc > 1 && !wcscmp(argv[1], L"--registry-and-failure")) { RegistryContract(argc > 2 && _wtoi(argv[2]) > 1); FailureContract(); return failures ? 1 : 0; }
    if (argc > 2 && !wcscmp(argv[1], L"--case")) return GuardContract(static_cast<ULONG>(_wtoi(argv[2])), argc > 3 && !wcscmp(argv[3], L"overflow")) ? 1 : 0;
    for (ULONG count = 0; count <= 2; ++count)
    {
        WCHAR command[32768]; swprintf_s(command, L"\"%s\" --case %lu", executable, count);
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION info = {};
        Check(CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, directory, &startup, &info) != FALSE, "start isolated case");
        if (info.hProcess) { DWORD code = 0; WaitForSingleObject(info.hProcess, 30000); GetExitCodeProcess(info.hProcess, &code); Check(code == 0, "isolated case passes"); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
    }
    for (ULONG count = 0; count <= 1; ++count)
    {
        WCHAR command[32768]; swprintf_s(command, L"\"%s\" --case %lu overflow", executable, count);
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup); PROCESS_INFORMATION info = {};
        Check(CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, 0, nullptr, directory, &startup, &info) != FALSE, "start overflow case");
        if (info.hProcess) { DWORD code = 0; WaitForSingleObject(info.hProcess, 10000); GetExitCodeProcess(info.hProcess, &code); Check(code == 0, "overflow case passes"); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
    }
    printf("native contracts failures=%d\n", failures); return failures ? 1 : 0;
}

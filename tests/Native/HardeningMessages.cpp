#include <Windows.h>
#include <cstdio>
#include <vector>
#include <cstring>

static int failures;
static void Check(bool ok, const char* name) { if (!ok) { printf("FAIL %s error=%lu\n", name, GetLastError()); ++failures; } }
static void CheckLines(bool ansiWindow, bool ansiMessage) {
    HWND window = ansiWindow ? CreateWindowExA(0, "EDIT", "ABCD", ES_MULTILINE, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr)
                             : CreateWindowExW(0, L"EDIT", L"ABCD", ES_MULTILINE, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
    Check(window != nullptr, "create edit");
    if (!window) return;
    for (USHORT capacity : { USHORT(0), USHORT(1), USHORT(2), USHORT(32767), USHORT(32768), USHORT(65535) }) {
        const size_t unit = ansiMessage ? sizeof(char) : sizeof(WCHAR);
        std::vector<BYTE> buffer((static_cast<size_t>(capacity) + 4) * unit + 2, 0x5A);
        memcpy(buffer.data(), &capacity, sizeof(capacity));
        auto expected = buffer;
        const size_t count = capacity < 4 ? capacity : 4;
        if (ansiMessage) memcpy(expected.data(), "ABCD", count);
        else memcpy(expected.data(), L"ABCD", count * unit);
        LRESULT result = ansiMessage ? SendMessageA(window, EM_GETLINE, 0, reinterpret_cast<LPARAM>(buffer.data()))
                                     : SendMessageW(window, EM_GETLINE, 0, reinterpret_cast<LPARAM>(buffer.data()));
        if (result != static_cast<LRESULT>(count) || buffer != expected) {
            printf("getline windowA=%d messageA=%d capacity=%u result=%ld expected=%zu procA=%p unicode=%d\n", ansiWindow, ansiMessage, capacity, result, count, reinterpret_cast<void*>(GetWindowLongPtrA(window,GWLP_WNDPROC)), IsWindowUnicode(window));
            Check(false, "EM_GETLINE count and untouched tail");
        }
    }
    DestroyWindow(window);
}
static void CheckPrivateImages() {
    SYSTEM_INFO system; GetSystemInfo(&system); ULONG_PTR cursor = 0; unsigned images = 0;
    while (cursor < reinterpret_cast<ULONG_PTR>(system.lpMaximumApplicationAddress)) {
        MEMORY_BASIC_INFORMATION region = {};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) || !region.RegionSize) break;
        if (region.Type == MEM_PRIVATE && region.BaseAddress == region.AllocationBase && region.State == MEM_COMMIT) {
            IMAGE_DOS_HEADER dos = {}; SIZE_T read = 0;
            if (ReadProcessMemory(GetCurrentProcess(), region.AllocationBase, &dos, sizeof(dos), &read) && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && dos.e_lfanew < 4096) {
                const auto base = reinterpret_cast<BYTE*>(region.AllocationBase); IMAGE_NT_HEADERS32 headers = {};
                if (ReadProcessMemory(GetCurrentProcess(), base + dos.e_lfanew, &headers, sizeof(headers), &read) && headers.Signature == IMAGE_NT_SIGNATURE) {
                    IMAGE_EXPORT_DIRECTORY exports = {}; char name[64] = {};
                    if (ReadProcessMemory(GetCurrentProcess(), base + headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, &exports, sizeof(exports), &read) &&
                        ReadProcessMemory(GetCurrentProcess(), base + exports.Name, name, sizeof(name)-1, &read) && !strcmp(name, "LocaleEmulator.dll")) {
                        ++images;
                        for (ULONG offset = 0; offset < headers.OptionalHeader.SizeOfImage;) {
                            MEMORY_BASIC_INFORMATION page = {}; if (!VirtualQuery(base + offset, &page, sizeof(page))) { Check(false,"query shadow page"); break; }
                            Check((page.Protect & 0xFF) != PAGE_EXECUTE_READWRITE && (page.Protect & 0xFF) != PAGE_EXECUTE_WRITECOPY, "private core no RWX");
                            offset += static_cast<ULONG>(page.RegionSize);
                        }
                        IMAGE_LOAD_CONFIG_DIRECTORY32 config = {}; DWORD cookie = 0;
                        Check(ReadProcessMemory(GetCurrentProcess(), base + headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress, &config, sizeof(config), &read) &&
                              ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(config.SecurityCookie), &cookie, sizeof(cookie), &read) && cookie && cookie != 0xBB40E64E, "private core official cookie initialized");
                    }
                }
            }
        }
        const ULONG_PTR next = reinterpret_cast<ULONG_PTR>(region.BaseAddress) + region.RegionSize;
        if (next <= cursor) break; cursor = next;
    }
    Check(images == 1, "one private core image");
    printf("private_core_images=%u\n", images);
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY policy = {};
    Check(GetProcessMitigationPolicy(GetCurrentProcess(), ProcessControlFlowGuardPolicy, &policy, sizeof(policy)) && policy.EnableControlFlowGuard, "CFG host mitigation active");
    printf("cfg_host_enabled=%lu\n", policy.EnableControlFlowGuard);
}
// 与 NativeProbe 一样，由 NativeLaunch 传入报告路径、目标 ACP 和可选工作目录。
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if ((argc != 3 && argc != 4) || (wcscmp(argv[2], L"932") && wcscmp(argv[2], L"936"))) {
        fputs("Usage: HardeningMessages.exe <output> <932|936> [working-directory]\n", stderr);
        return 128;
    }
    FILE* file = nullptr;
    if (_wfreopen_s(&file, argv[1], L"w", stdout) || !file) return 128;
    const UINT expectedCodePage = !wcscmp(argv[2], L"932") ? 932 : 936;
    Check(GetACP() == expectedCodePage, "requested ACP");
    if (argc == 4) {
        WCHAR directory[MAX_PATH];
        const DWORD length = GetCurrentDirectoryW(MAX_PATH, directory);
        Check(length && length < MAX_PATH && !_wcsicmp(directory, argv[3]), "working directory");
    }
    for (bool window : {false, true}) CheckLines(window, window);
    CheckPrivateImages();
    printf("acp=%u getline_cases=12 failures=%d\n", GetACP(), failures);
    return failures ? 1 : 0;
}

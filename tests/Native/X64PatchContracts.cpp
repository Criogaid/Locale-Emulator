#include "../../Native/LocaleEmulatorX64/LocaleEmulatorPlus/support/Support.h"

static int failures;

static bool Check(bool condition, const char* name)
{
    if (!condition)
    {
        printf("FAIL %s error=%lu\n", name, GetLastError());
        ++failures;
    }
    return condition;
}

static void RipRelativeComparePreservesTarget()
{
    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    const SIZE_T pageSize = system.dwPageSize;
    constexpr SIZE_T RelocatedOffset = 256;
    constexpr SIZE_T CompareLength = 7;
    auto memory = static_cast<BYTE*>(VirtualAlloc(nullptr, pageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!Check(memory != nullptr, "RipRelativeCompare allocation"))
        return;

    BYTE* original = memory;
    BYTE* relocated = memory + RelocatedOffset;
    auto realData = reinterpret_cast<volatile DWORD*>(memory + pageSize);
    auto wrongData = reinterpret_cast<volatile DWORD*>(memory + pageSize + RelocatedOffset);
    // 错误位移也指向有效数据，以相反的值稳定暴露语义错误，避免访问异常。
    const BYTE code[] = {
        0x83, 0x3D, 0, 0, 0, 0, 0x00, // cmp dword ptr [rip+disp32], 0
        0x0F, 0x94, 0xC0,             // setz al
        0x0F, 0xB6, 0xC0,             // movzx eax, al
        0xC3                          // ret
    };
    memcpy(original, code, sizeof(code));
    const LONG displacement = static_cast<LONG>(pageSize - CompareLength);
    memcpy(original + 2, &displacement, sizeof(displacement));

    ULONG_PTR targetLength = 0;
    ULONG_PTR sourceLength = 0;
    const NTSTATUS status = CopyOneOpCode(relocated, original, &targetLength,
        &sourceLength, CompareLength, 0);
    printf("RipRelativeCompare copy status=%08lx sourceLength=%zu targetLength=%zu\n",
        status, sourceLength, targetLength);
    if (Check(NT_SUCCESS(status), "RipRelativeCompare CopyOneOpCode succeeds") &&
        Check(sourceLength == CompareLength && targetLength == CompareLength,
            "RipRelativeCompare instruction lengths"))
    {
        memcpy(relocated + targetLength, code + CompareLength, sizeof(code) - CompareLength);
        DWORD previous = 0;
        // 仅代码页变为 RX；数据页保持 RW，允许同一代码验证两种输入。
        if (Check(VirtualProtect(memory, pageSize, PAGE_EXECUTE_READ, &previous) != FALSE,
                "RipRelativeCompare code page RX") &&
            Check(FlushInstructionCache(GetCurrentProcess(), memory, pageSize) != FALSE,
                "RipRelativeCompare flush instruction cache"))
        {
            using CompareFunction = DWORD (*)();
            const auto originalFunction = reinterpret_cast<CompareFunction>(original);
            const auto relocatedFunction = reinterpret_cast<CompareFunction>(relocated);
            for (DWORD value = 0; value <= 1; ++value)
            {
                *realData = value;
                *wrongData = 1 - value;
                const DWORD expected = value == 0 ? 1 : 0;
                const DWORD originalResult = originalFunction();
                const DWORD relocatedResult = relocatedFunction();
                printf("RipRelativeCompare data=%lu wrongData=%lu expected=%lu original=%lu relocated=%lu\n",
                    value, 1 - value, expected, originalResult, relocatedResult);
                Check(originalResult == expected, "RipRelativeCompare original reads real data");
                Check(relocatedResult == expected, "RipRelativeCompare relocated reads real data");
            }
        }
    }
    Check(VirtualFree(memory, 0, MEM_RELEASE) != FALSE, "RipRelativeCompare release allocation");
}

static void PrefixedRipRelativeNegativeDisplacements()
{
    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    const SIZE_T pageSize = system.dwPageSize;
    auto memory = static_cast<BYTE*>(VirtualAlloc(nullptr, pageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!Check(memory != nullptr, "PrefixedRipRelative allocation"))
        return;
    BYTE* code = memory + pageSize;
    constexpr SIZE_T InstructionLength = 7;
    const BYTE opcodes[] = { 0x8B, 0x8D };
    bool ready = true;
    for (SIZE_T index = 0; index < sizeof(opcodes); ++index)
    {
        BYTE* source = code + index * 32;
        BYTE* target = source + 256;
        BYTE* data = memory + index * sizeof(ULONGLONG);
        const ULONGLONG value = 0x123456789ABCDEF0ull;
        memcpy(data, &value, sizeof(value));
        const BYTE instruction[] = { 0x48, opcodes[index], 0x05, 0, 0, 0, 0, 0xC3 };
        memcpy(source, instruction, sizeof(instruction));
        const LONG displacement = static_cast<LONG>(data - (source + InstructionLength));
        memcpy(source + 3, &displacement, sizeof(displacement));
        ULONG_PTR targetLength = 0, sourceLength = 0;
        const NTSTATUS status = CopyOneOpCode(target, source, &targetLength, &sourceLength, InstructionLength, 0);
        printf("PrefixedRipRelative opcode=%02x displacement=%ld status=%08lx\n", opcodes[index], displacement, status);
        ready = Check(NT_SUCCESS(status) && targetLength == InstructionLength &&
            sourceLength == InstructionLength, "PrefixedRipRelative copy succeeds") && ready;
        target[InstructionLength] = 0xC3;
    }
    DWORD previous = 0;
    if (ready && Check(VirtualProtect(code, pageSize, PAGE_EXECUTE_READ, &previous) != FALSE,
            "PrefixedRipRelative code page RX") &&
        Check(FlushInstructionCache(GetCurrentProcess(), code, pageSize) != FALSE,
            "PrefixedRipRelative flush instruction cache"))
    {
        using Function = ULONG_PTR (*)();
        for (SIZE_T index = 0; index < sizeof(opcodes); ++index)
        {
            const ULONG_PTR expected = index == 0 ? 0x123456789ABCDEF0ull :
                reinterpret_cast<ULONG_PTR>(memory + index * sizeof(ULONGLONG));
            Check(reinterpret_cast<Function>(code + index * 32)() == expected,
                "PrefixedRipRelative original result");
            Check(reinterpret_cast<Function>(code + index * 32 + 256)() == expected,
                "PrefixedRipRelative relocated result");
        }
    }
    Check(VirtualFree(memory, 0, MEM_RELEASE) != FALSE, "PrefixedRipRelative release allocation");
}

static void RejectedRelocationsLeaveDestinationUntouched()
{
    BYTE source[] = { 0x83, 0x3D, 0, 0, 0, 0, 0 };
    BYTE addressOverride[] = { 0x67, 0x8B, 0x05, 0, 0, 0, 0 };
    BYTE destination[32];
    BYTE expected[sizeof(destination)];
    memset(expected, 0xA5, sizeof(expected));
    memcpy(destination, expected, sizeof(destination));
    ULONG_PTR targetLength = 0, sourceLength = 0;
    // 显式执行地址只参与位移计算，不解引用；与源地址相距 4 GiB。
    PVOID farIp = reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(source) + 0x100000000ull);
    NTSTATUS status = CopyOneOpCode(destination, source, &targetLength, &sourceLength, sizeof(source), 0, farIp);
    printf("RejectedRelocations outOfRangeStatus=%08lx\n", status);
    Check(!NT_SUCCESS(status), "RejectedRelocations out of range fails");
    Check(memcmp(destination, expected, sizeof(destination)) == 0,
        "RejectedRelocations out of range leaves destination untouched");
    status = CopyOneOpCode(destination, addressOverride, &targetLength, &sourceLength, sizeof(addressOverride), 0);
    printf("RejectedRelocations addressOverrideStatus=%08lx\n", status);
    Check(status == STATUS_NOT_SUPPORTED, "RejectedRelocations address size override is unsupported");
    Check(memcmp(destination, expected, sizeof(destination)) == 0,
        "RejectedRelocations address size override leaves destination untouched");
}

int wmain()
{
    static_assert(sizeof(void*) == 8, "X64PatchContracts requires a 64-bit test host");
    RipRelativeComparePreservesTarget();
    PrefixedRipRelativeNegativeDisplacements();
    RejectedRelocationsLeaveDestinationUntouched();
    printf("x64 patch contract failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}

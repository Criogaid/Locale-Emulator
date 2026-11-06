#include "../../Native/LocaleEmulator/ml.h"
#include <stdio.h>

namespace {
bool failAllocation;
LONG constructed, destroyed, allocated, freed;
size_t lastSize;
PVOID TestAllocate(size_t size)
{
    lastSize = size;
    if (failAllocation) return nullptr;
    PVOID block = HeapAlloc(GetProcessHeap(), 0, size);
    if (block != nullptr) ++allocated;
    return block;
}
VOID TestFree(PVOID block)
{
    ++freed;
    if (!HeapFree(GetProcessHeap(), 0, block)) ::ExitProcess(100);
}
}

// 只在本翻译单元替换分配后端，直接验证产品共享重载。
#define AllocateMemoryP TestAllocate
#define FreeMemoryP TestFree
ML_OVERLOAD_NEW
#undef FreeMemoryP
#undef AllocateMemoryP

struct Object {
    Object() { ++constructed; }
    ~Object() { ++destroyed; }
};
__declspec(noinline) Object* MakeObject() { return new (std::nothrow) Object; }
__declspec(noinline) Object* MakeArray() { return new (std::nothrow) Object[3]; }

int __cdecl main()
{
    failAllocation = true;
    if (MakeObject() != nullptr || MakeArray() != nullptr || constructed != 0) return 1;
    failAllocation = false;
    Object* object = MakeObject();
    if (object == nullptr || constructed != 1) return 2;
    delete object;
    Object* array = MakeArray();
    if (array == nullptr || constructed != 4) return 3;
    delete[] array;
    if (destroyed != 4 || freed != 2) return 4;

    void* zero = ::operator new(0, std::nothrow);
    if (zero == nullptr || lastSize != 0) return 5;
    void* secondZero = ::operator new[](0, std::nothrow);
    if (secondZero == nullptr || secondZero == zero || lastSize != 0) return 6;
    ::operator delete(zero, std::nothrow);
    ::operator delete[](secondZero, std::nothrow);

    void* scalar = ::operator new(16, std::nothrow);
    void* bytes = ::operator new[](16, std::nothrow);
    if (scalar == nullptr || bytes == nullptr) return 7;
    ::operator delete(scalar, size_t(16));
    ::operator delete[](bytes, size_t(16));
    ::operator delete(nullptr);
    ::operator delete[](nullptr, std::nothrow);
    if (allocated != 6 || freed != allocated) return 8;
    puts("allocation_failure=null constructor_skipped=ok retry=ok zero_size=ok destructors=ok delete_pairs=ok");
    return 0;
}

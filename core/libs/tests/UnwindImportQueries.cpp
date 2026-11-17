#include "Testing.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

namespace {

std::atomic<long> queries {0};

SIZE_T WINAPI CountedQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION memory, SIZE_T size) {
    ++queries;
    return VirtualQuery(address, memory, size);
}

class QueryPatch {
public:
    QueryPatch() {
        const HMODULE module = GetModuleHandleW(L"libc.prx");
        Require(module != nullptr, "libc module not found");
        const auto base = reinterpret_cast<BYTE*>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        Require(dos->e_magic == IMAGE_DOS_SIGNATURE, "libc DOS signature invalid");
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        Require(nt->Signature == IMAGE_NT_SIGNATURE, "libc PE signature invalid");
        const auto importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        Require(importRva != 0, "libc imports missing");
        const auto realQuery = reinterpret_cast<ULONG_PTR>(&VirtualQuery);
        for (auto import = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + importRva); import->Name != 0; ++import) {
            for (auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + import->FirstThunk); thunk->u1.Function != 0; ++thunk) {
                if (thunk->u1.Function != realQuery) continue;
                Require(slot == nullptr, "multiple libc VirtualQuery IAT entries found");
                slot = &thunk->u1.Function;
            }
        }
        Require(slot != nullptr, "libc VirtualQuery IAT entry not found");
        original = *slot;
        replace(reinterpret_cast<ULONG_PTR>(&CountedQuery));
    }

    ~QueryPatch() {
        replace(original);
    }

    QueryPatch(const QueryPatch&) = delete;
    QueryPatch& operator=(const QueryPatch&) = delete;

private:
    void replace(ULONG_PTR value) {
        DWORD protection = 0;
        Require(VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection), "make VirtualQuery IAT entry writable failed");
        *slot = value;
        DWORD previous = 0;
        Require(VirtualProtect(slot, sizeof(*slot), protection, &previous), "restore VirtualQuery IAT protection failed");
    }

    ULONG_PTR* slot = nullptr;
    ULONG_PTR original = 0;
};

[[gnu::noinline]] void Raise(int value) {
    throw value;
}

[[gnu::noinline]] int Caught(int value) {
    try {
        Raise(value);
    } catch (int caught) {
        return caught;
    }
    return -1;
}

}

int main() {
    QueryPatch patch;
    Require(Caught(1) == 1, "the first exception is caught");
    Require(queries.load() > 0, "the first unwind checks that the personality import is readable");
    Require(Caught(2) == 2, "the second exception is caught");
    const long warm = queries.load();
    for (int value = 3; value < 259; ++value) Require(Caught(value) == value, "a repeated exception is caught");
    Require(queries.load() == warm, "repeated unwinds through one image do not query its pages again");
    std::printf("unwind import query test passed\n");
    return 0;
}

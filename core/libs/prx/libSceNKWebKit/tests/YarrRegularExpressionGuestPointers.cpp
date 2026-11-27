#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Jsc::WtfAbi::StringImpl;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;
using Jsc::YarrAbi::StringReference;

const auto Construct = &_ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE;
const auto Destroy = &_ZN3JSC4Yarr17RegularExpressionD1Ev;
const auto DestroyBase = &_ZN3JSC4Yarr17RegularExpressionD2Ev;
const auto Assign = &_ZN3JSC4Yarr17RegularExpressionaSERKS1_;

std::set<void*> live;
bool throwOnNextFree = false;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer) return;
    if (live.erase(pointer) != 1) std::abort();
    std::free(pointer);
    if (throwOnNextFree) {
        throwOnNextFree = false;
        throw std::runtime_error("simulated guest free failure");
    }
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    if (pointer) std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return *pointer ? 0 : 12;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Yarr RegularExpression guest pointer check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

RegularExpression Make(const std::string& pattern) {
    StringImpl impl{2, static_cast<unsigned>(pattern.size()), pattern.data(), 4};
    const StringReference reference{&impl};
    RegularExpression expression{};
    Construct(&expression, &reference, 0, 0, 0);
    return expression;
}

template<typename TCall>
bool ThrowsNotImplemented(TCall call) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    try {
        call();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return NotImplementedCalls_nid_no_patch() == before + 1;
    } catch (...) {
        return false;
    }
    return false;
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadWriteToReadOnly() const {
        return reinterpret_cast<T*>(base + pageSize - sizeof(T) / 2);
    }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

void TestGuestPointers() {
    GuardedPages pages;
    auto* unmappedExpr = reinterpret_cast<RegularExpression*>(pages.Guard());
    auto* readOnlyExpr = reinterpret_cast<RegularExpression*>(pages.ReadOnly());
    auto* splitExprRwRo = pages.SplitReadWriteToReadOnly<RegularExpression>();
    const auto* splitExprRoGuard = pages.SplitReadOnlyToGuard<const RegularExpression>();
    auto* unmappedPrivate = reinterpret_cast<RegularExpressionPrivate*>(pages.Guard());
    auto* readOnlyPrivate = reinterpret_cast<RegularExpressionPrivate*>(pages.ReadOnly());

    Require(ThrowsNotImplemented([&] { Destroy(unmappedExpr); }));
    Require(ThrowsNotImplemented([&] { Destroy(const_cast<RegularExpression*>(splitExprRoGuard)); }));
    Require(ThrowsNotImplemented([&] { DestroyBase(unmappedExpr); }));
    Require(ThrowsNotImplemented([&] { DestroyBase(const_cast<RegularExpression*>(splitExprRoGuard)); }));

    RegularExpression badTarget{unmappedPrivate};
    Require(ThrowsNotImplemented([&] { Destroy(&badTarget); }));
    Require(ThrowsNotImplemented([&] { DestroyBase(&badTarget); }));
    RegularExpression roTarget{readOnlyPrivate};
    Require(ThrowsNotImplemented([&] { Destroy(&roTarget); }));

    RegularExpression valid = Make("c+");
    RegularExpressionPrivate* const validD = valid.d;
    Require(validD->refCount == 1);

    Require(ThrowsNotImplemented([&] { Assign(unmappedExpr, &valid); }));
    Require(ThrowsNotImplemented([&] { Assign(readOnlyExpr, &valid); }));
    const std::size_t splitHead = sizeof(RegularExpression) / 2;
    std::memset(splitExprRwRo, 0x5A, splitHead);
    Require(ThrowsNotImplemented([&] { Assign(splitExprRwRo, &valid); }));
    for (std::size_t i = 0; i < splitHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitExprRwRo)[i] == 0x5A);
    Require(validD->refCount == 1);

    Require(ThrowsNotImplemented([&] { Assign(&valid, unmappedExpr); }));
    Require(ThrowsNotImplemented([&] { Assign(&valid, splitExprRoGuard); }));
    Require(valid.d == validD && validD->refCount == 1);

    Require(ThrowsNotImplemented([&] { Assign(&valid, &badTarget); }));
    Require(ThrowsNotImplemented([&] { Assign(&valid, &roTarget); }));
    Require(valid.d == validD && validD->refCount == 1);

    Require(ThrowsNotImplemented([&] { Assign(&badTarget, &valid); }));
    Require(badTarget.d == unmappedPrivate && validD->refCount == 1);
    Require(ThrowsNotImplemented([&] { Assign(&roTarget, &valid); }));
    Require(roTarget.d == readOnlyPrivate && validD->refCount == 1);

    RegularExpression dst = Make("a+");
    RegularExpressionPrivate* const oldD = dst.d;
    Require(oldD != validD && oldD->refCount == 1);
    throwOnNextFree = true;
    Require(Assign(&dst, &valid) == &dst);
    Require(!throwOnNextFree);
    Require(dst.d == validD && validD->refCount == 2);
    Destroy(&dst);
    Require(validD->refCount == 1);

    Destroy(&valid);
}

}  // namespace

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestGuestPointers();
    Require(live.empty());
    return 0;
}

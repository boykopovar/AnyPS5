#include "prx/libc/include/General.hpp"
#include "prx/libSceJsc/include/ContentSearchUtilitiesExports.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector lineEndings check failed at line %d\n", line);
        std::exit(1);
    }
}

#define Require(value) Check((value), __LINE__)

std::set<void*> liveAllocations;
unsigned allocateCalls = 0;
unsigned deallocateCalls = 0;
std::size_t lastAllocateSize = 0;
bool failNext = false;
unsigned char* protectPageOnNextAllocate = nullptr;
std::size_t protectSizeOnNextAllocate = 0;

void SetPageReadOnly(unsigned char* address, std::size_t size) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(address, size, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(address, size, PROT_READ) == 0);
#endif
}

void* TestAllocate(std::size_t size) {
    ++allocateCalls;
    lastAllocateSize = size;
    if (protectPageOnNextAllocate) {
        SetPageReadOnly(protectPageOnNextAllocate, protectSizeOnNextAllocate);
        protectPageOnNextAllocate = nullptr;
        protectSizeOnNextAllocate = 0;
    }
    if (failNext) {
        failNext = false;
        return nullptr;
    }
    void* ptr = std::malloc(size);
    liveAllocations.insert(ptr);
    return ptr;
}

void TestDeallocate(void* ptr) {
    ++deallocateCalls;
    Require(liveAllocations.erase(ptr) == 1);
    std::free(ptr);
}

const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};

using Endings = std::vector<std::size_t>;

struct Result {
    Endings values;
    unsigned capacity;
};

SizeVector Garbage() {
    return {reinterpret_cast<std::size_t*>(static_cast<std::uintptr_t>(0x1234)), 77, 99};
}

Result Call(const String& string) {
    SizeVector vector = Garbage();
    const unsigned allocations = allocateCalls;
    const unsigned releases = deallocateCalls;
    Require(_ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &string) == &vector);
    Require(allocateCalls == allocations + 1 && deallocateCalls == releases);
    Require(vector.buffer != nullptr && vector.size <= vector.capacity);
    Require(lastAllocateSize == static_cast<std::size_t>(vector.capacity) * sizeof(std::size_t));
    Result result{Endings(vector.buffer, vector.buffer + vector.size), vector.capacity};
    TestDeallocate(vector.buffer);
    return result;
}

Result Call8(const std::string& text) {
    StringImpl impl{2, static_cast<unsigned>(text.size()), text.data(), StringImplFlag8Bit};
    const String string{&impl};
    const Result result = Call(string);
    Require(impl.refCount == 2);
    return result;
}

Result Call16(const std::u16string& text) {
    StringImpl impl{2, static_cast<unsigned>(text.size()), text.data(), 0};
    const String string{&impl};
    return Call(string);
}

std::string Lines(unsigned count) {
    std::string text;
    for (unsigned index = 0; index < count; ++index) text += "x\n";
    text += "y";
    return text;
}

template<typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void TestEndings() {
    const String null{nullptr};
    Require(Call(null).values == Endings({0}));
    Require(Call8("").values == Endings({0}));
    Require(Call8("abc").values == Endings({3, 3}));
    Require(Call8("a\nb").values == Endings({2, 3, 3}));
    Require(Call8("a\n").values == Endings({2, 2}));
    Require(Call8("\n").values == Endings({1, 1}));
    Require(Call8("\n\n").values == Endings({1, 2, 2}));
    Require(Call8("\na").values == Endings({1, 2, 2}));
    Require(Call8("ab\n\ncd\ne").values == Endings({3, 4, 7, 8, 8}));
    Require(Call8("a\rb\r\nc").values == Endings({5, 6, 6}));
    Require(Call8(std::string("a\0\nb", 4)).values == Endings({3, 4, 4}));

    Require(Call16(u"Ċਊ").values == Endings({2, 2}));
    Require(Call16(u"α\nβ").values == Endings({2, 3, 3}));
    Require(Call16(u"αĊ\n").values == Endings({3, 3}));
    Require(Call16(u"").values == Endings({0}));
}

void TestCapacity() {
    Require(Call8("").capacity == 16);
    Require(Call8("a\nb").capacity == 16);

    const Result full = Call8(Lines(14));
    Require(full.values.size() == 16 && full.capacity == 16);
    Require(full.values[0] == 2 && full.values[13] == 28 && full.values[14] == 29 && full.values[15] == 29);

    const Result grown = Call8(Lines(15));
    Require(grown.values.size() == 17 && grown.capacity == 21);
    Require(Call8(Lines(19)).capacity == 21);
    Require(Call8(Lines(20)).capacity == 27);
    Require(Call8(Lines(25)).capacity == 27);
    Require(Call8(Lines(26)).capacity == 34);
    Require(Call8(Lines(32)).capacity == 34);
    Require(Call8(Lines(33)).capacity == 43);

    const Result large = Call8(Lines(1000));
    Require(large.values.size() == 1002 && large.capacity == 1031);
    for (unsigned index = 0; index < 1000; ++index) Require(large.values[index] == 2 * (index + 1));
    Require(large.values[1000] == 2001 && large.values[1001] == 2001);
}

void TestFailures() {
    const std::string text = "a\nb";
    StringImpl impl{2, 3, text.data(), StringImplFlag8Bit};
    const String string{&impl};
    SizeVector vector = Garbage();

    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(nullptr, &string); }));
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, nullptr); }));

    StringImpl missing{2, 3, nullptr, StringImplFlag8Bit};
    const String missingString{&missing};
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &missingString); }));

    const unsigned allocations = allocateCalls;
    failNext = true;
    Require(Throws<std::bad_alloc>([&] { _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &string); }));
    Require(allocateCalls == allocations + 1);
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);

    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    Require(Throws<std::runtime_error>([&] { _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &string); }));
    Require(allocateCalls == allocations + 1);
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);
    RegisterPersistenceAllocator_nid_no_patch(&allocator);
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

    unsigned char* Writable() const { return base; }
    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename Function>
void RequireNotImplemented(Function&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

void TestGuestPointers() {
    GuardedPages pages;
    const std::string text = "a\nb";
    StringImpl validImpl{2, 3, text.data(), StringImplFlag8Bit};
    const String validString{&validImpl};
    SizeVector vector = Garbage();
    const unsigned allocationsBefore = allocateCalls;

    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(
            reinterpret_cast<SizeVector*>(pages.Guard()), &validString);
    });
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(
            reinterpret_cast<SizeVector*>(pages.ReadOnly()), &validString);
    });
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(
            reinterpret_cast<SizeVector*>(pages.Guard() - sizeof(SizeVector) / 2), &validString);
    });
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(
            &vector, reinterpret_cast<const String*>(pages.Guard()));
    });
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);

    const String badImpl{reinterpret_cast<StringImpl*>(pages.Guard())};
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &badImpl);
    });
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);

    StringImpl badData8{2, 8, pages.Guard(), StringImplFlag8Bit};
    const String badData8String{&badData8};
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &badData8String);
    });
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);

    StringImpl splitData16{2, 4, pages.Guard() - sizeof(char16_t), 0};
    const String splitData16String{&splitData16};
    RequireNotImplemented([&] {
        _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(&vector, &splitData16String);
    });
    Require(vector.buffer == Garbage().buffer && vector.capacity == 77 && vector.size == 99);
    Require(allocateCalls == allocationsBefore);

    {
        unsigned char* twoPages = nullptr;
#ifdef _WIN32
        twoPages = static_cast<unsigned char*>(VirtualAlloc(nullptr, pages.pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(twoPages != nullptr);
#else
        void* mapped = mmap(nullptr, pages.pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        twoPages = static_cast<unsigned char*>(mapped);
#endif
        auto* splitResult = reinterpret_cast<SizeVector*>(twoPages + pages.pageSize - sizeof(std::size_t));
        const SizeVector sentinel = Garbage();
        std::memcpy(splitResult, &sentinel, sizeof(sentinel));
        protectPageOnNextAllocate = twoPages + pages.pageSize;
        protectSizeOnNextAllocate = pages.pageSize;
        const unsigned releasesBefore = deallocateCalls;
        RequireNotImplemented([&] {
            _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(splitResult, &validString);
        });
        Require(protectPageOnNextAllocate == nullptr);
        Require(deallocateCalls == releasesBefore + 1);
        Require(std::memcmp(splitResult, &sentinel, sizeof(sentinel)) == 0);
        Require(liveAllocations.empty());
#ifdef _WIN32
        VirtualFree(twoPages, 0, MEM_RELEASE);
#else
        munmap(twoPages, pages.pageSize * 2);
#endif
    }
}

}  // namespace

int main() {
    RegisterPersistenceAllocator_nid_no_patch(&allocator);
    TestEndings();
    TestCapacity();
    TestFailures();
    TestGuestPointers();
    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    Require(liveAllocations.empty());
    return 0;
}

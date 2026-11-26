#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <csetjmp>
#include <cstring>
#include <stdexcept>
#include <typeinfo>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

thread_local int destroyed = 0;
struct Tracked {
    int value;
    ~Tracked() { ++destroyed; }
};

thread_local int terminateRan = 0;
thread_local std::jmp_buf terminateJump;

void CustomTerminate() {
    terminateRan = 1;
    std::longjmp(terminateJump, 1);
}

struct UnwindObserver {
    unsigned expectedUncaught;
    ~UnwindObserver() {
        assert(__cxa_uncaught_exceptions_nid_postfix() == expectedUncaught);
    }
};

void CheckRethrowInCatch() {
    destroyed = 0;
    const void* innerAddr = nullptr;
    try {
        UnwindObserver outerGuard{1};
        try {
            throw Tracked{42};
        } catch (const Tracked& caught) {
            assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
            innerAddr = &caught;
            __cxa_rethrow_nid_postfix();
        }
    } catch (const Tracked& outer) {
        assert(&outer == innerAddr);
        assert(destroyed == 0);
    }
    assert(destroyed == 1);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
}

void CheckRethrowNoActiveException() {
    terminateRan = 0;
    void (*oldHandler)() = _ZSt13set_terminatePFvvE_nid_postfix(CustomTerminate);
    assert(oldHandler != nullptr);

    if (setjmp(terminateJump) == 0) {
        __cxa_rethrow_nid_postfix();
        assert(false);
    }

    assert(terminateRan == 1);
    _ZSt13set_terminatePFvvE_nid_postfix(oldHandler);
}

void CheckDependentExceptionAllocateFree() {
    void* storage = __cxa_allocate_dependent_exception_nid_postfix();
    assert(storage != nullptr);

    auto* header = static_cast<LibcException::Header*>(storage);
    assert(header->type == nullptr);
    assert(header->destructor == nullptr);
    assert(header->unexpected == nullptr);
    assert(header->terminate == nullptr);
    assert(header->next == nullptr);
    assert(header->handlers == 0);
    assert(header->selector == 0);
    assert(header->_pad == 0);
    assert(header->_pad2 == 0);
    assert(header->action == nullptr);
    assert(header->lsda == nullptr);
    assert(header->landing == 0);
    assert(header->adjusted == nullptr);
    assert(header->unwind.exception_class == 0);
    assert(header->unwind.exception_cleanup == nullptr);

    __cxa_free_dependent_exception_nid_postfix(storage);
    __cxa_free_dependent_exception_nid_postfix(nullptr);
}

void CheckRethrowPrimaryException() {
    destroyed = 0;
    void* primary = nullptr;

    try {
        throw Tracked{99};
    } catch (const Tracked&) {
        primary = __cxa_current_primary_exception_nid_postfix();
        assert(primary != nullptr);
        assert(destroyed == 0);
    }

    assert(destroyed == 0);

    __cxa_increment_exception_refcount_nid_postfix(primary);
    assert(destroyed == 0);

    try {
        __cxa_rethrow_primary_exception_nid_postfix(primary);
        assert(false);
    } catch (const Tracked& caught) {
        assert(caught.value == 99);
        assert(&caught == primary);
        assert(destroyed == 0);
    }

    assert(destroyed == 0);

    __cxa_decrement_exception_refcount_nid_postfix(primary);
    assert(destroyed == 0);

    __cxa_decrement_exception_refcount_nid_postfix(primary);
    assert(destroyed == 1);

    terminateRan = 0;
    void (*oldHandler)() = _ZSt13set_terminatePFvvE_nid_postfix(CustomTerminate);
    assert(oldHandler != nullptr);
    if (setjmp(terminateJump) == 0) {
        __cxa_rethrow_primary_exception_nid_postfix(nullptr);
    } else {
        assert(false);
    }
    assert(terminateRan == 0);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    _ZSt13set_terminatePFvvE_nid_postfix(oldHandler);
}

constexpr std::uint32_t IosBadBit = 0x1;
constexpr std::uint32_t IosEofBit = 0x2;
constexpr std::uint32_t IosFailBit = 0x4;
constexpr std::size_t IosStateOffset = 32;
constexpr std::size_t IosExceptionsOffset = 36;

struct alignas(8) GuestIosBase {
    unsigned char bytes[0x88];
};

GuestIosBase MakeIosBase(std::uint32_t state, std::uint32_t exceptions) {
    GuestIosBase ios{};
    for (std::size_t index = 0; index < sizeof(ios.bytes); ++index) ios.bytes[index] = static_cast<unsigned char>(0xA0 + index);
    std::memcpy(ios.bytes + IosStateOffset, &state, sizeof(state));
    std::memcpy(ios.bytes + IosExceptionsOffset, &exceptions, sizeof(exceptions));
    return ios;
}

std::uint32_t IosWord(const GuestIosBase& ios, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, ios.bytes + offset, sizeof(value));
    return value;
}

void CheckIosNeighbours(const GuestIosBase& ios) {
    for (std::size_t index = 0; index < sizeof(ios.bytes); ++index) {
        if (index >= IosStateOffset && index < IosExceptionsOffset + sizeof(std::uint32_t)) continue;
        assert(ios.bytes[index] == static_cast<unsigned char>(0xA0 + index));
    }
}

void CheckIosSetBadbitWithoutRethrow() {
    auto ios = MakeIosBase(IosEofBit | IosFailBit, 0);
    destroyed = 0;
    try {
        throw Tracked{1};
    } catch (...) {
        _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(&ios);
    }
    assert(destroyed == 1);
    assert(IosWord(ios, IosStateOffset) == (IosEofBit | IosFailBit | IosBadBit));
    assert(IosWord(ios, IosExceptionsOffset) == 0);
    CheckIosNeighbours(ios);

    ios = MakeIosBase(0, 0);
    _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(&ios);
    assert(IosWord(ios, IosStateOffset) == IosBadBit);
    assert(IosWord(ios, IosExceptionsOffset) == 0);
    CheckIosNeighbours(ios);

    ios = MakeIosBase(IosBadBit, IosFailBit | IosEofBit);
    destroyed = 0;
    try {
        throw Tracked{2};
    } catch (const Tracked&) {
        _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(&ios);
    }
    assert(destroyed == 1);
    assert(IosWord(ios, IosStateOffset) == IosBadBit);
    assert(IosWord(ios, IosExceptionsOffset) == (IosFailBit | IosEofBit));
    CheckIosNeighbours(ios);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
}

void CheckIosSetBadbitRethrows(std::uint32_t mask) {
    auto ios = MakeIosBase(IosEofBit, mask);
    destroyed = 0;
    const void* innerAddr = nullptr;
    try {
        try {
            throw Tracked{3};
        } catch (const Tracked& caught) {
            innerAddr = &caught;
            _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(&ios);
            assert(false);
        }
    } catch (const Tracked& outer) {
        assert(&outer == innerAddr);
        assert(outer.value == 3);
        assert(destroyed == 0);
    }
    assert(destroyed == 1);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    assert(IosWord(ios, IosStateOffset) == (IosEofBit | IosBadBit));
    assert(IosWord(ios, IosExceptionsOffset) == mask);
    CheckIosNeighbours(ios);
}

void CheckIosSetBadbitRethrowWithoutException() {
    static GuestIosBase ios;
    ios = MakeIosBase(0, IosBadBit);
    terminateRan = 0;
    void (*oldHandler)() = _ZSt13set_terminatePFvvE_nid_postfix(CustomTerminate);
    if (setjmp(terminateJump) == 0) {
        _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(&ios);
        assert(false);
    }
    assert(terminateRan == 1);
    _ZSt13set_terminatePFvvE_nid_postfix(oldHandler);
    assert(IosWord(ios, IosStateOffset) == IosBadBit);
    CheckIosNeighbours(ios);
}

void CheckIosSetBadbitNullObject() {
    bool threw = false;
    try {
        _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);

    threw = false;
    try {
        _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(reinterpret_cast<void*>(0x1000));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
}

class ReadOnlyPage {
    unsigned char* allocation;
    std::size_t size;
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        assert(allocation != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        assert(page > 0);
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        assert(allocation != MAP_FAILED);
#endif
    }
    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, size);
#endif
    }
    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;
    GuestIosBase* Place(const GuestIosBase& ios) {
        std::memcpy(allocation, &ios, sizeof(ios));
#ifdef _WIN32
        DWORD previous;
        assert(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0);
#else
        assert(mprotect(allocation, size, PROT_READ) == 0);
#endif
        return reinterpret_cast<GuestIosBase*>(allocation);
    }
};

void CheckIosSetBadbitReadOnlyObject() {
    ReadOnlyPage page;
    auto* ios = page.Place(MakeIosBase(IosEofBit, IosBadBit));
    destroyed = 0;
    bool threw = false;
    try {
        try {
            throw Tracked{4};
        } catch (const Tracked&) {
            _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(ios);
            assert(false);
        }
    } catch (const std::invalid_argument&) {
        threw = true;
    } catch (const Tracked&) {
        assert(false);
    }
    assert(threw);
    assert(destroyed == 1);
    assert(__cxa_uncaught_exceptions_nid_postfix() == 0);
    assert(IosWord(*ios, IosStateOffset) == IosEofBit);
    assert(IosWord(*ios, IosExceptionsOffset) == IosBadBit);
    CheckIosNeighbours(*ios);
}

int main() {
    CheckRethrowInCatch();
    CheckRethrowNoActiveException();
    CheckDependentExceptionAllocateFree();
    CheckRethrowPrimaryException();
    CheckIosSetBadbitWithoutRethrow();
    CheckIosSetBadbitRethrows(IosBadBit);
    CheckIosSetBadbitRethrows(IosBadBit | IosFailBit);
    CheckIosSetBadbitRethrowWithoutException();
    CheckIosSetBadbitNullObject();
    CheckIosSetBadbitReadOnlyObject();
    std::puts("guest libc rethrow tests passed");
    return 0;
}

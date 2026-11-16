#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Queue pointer guard failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

struct Links {
    void* forward;
    void* back;
};

class Pages {
    unsigned char* allocation;
    std::size_t size;
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(allocation != nullptr);
        DWORD previous;
        Check(VirtualProtect(allocation + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Check(page > 0);
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Check(allocation != MAP_FAILED);
        Check(mprotect(allocation + size, size, PROT_NONE) == 0);
#endif
        char value = 0;
        Check(!GuestCopyIn_nid_no_patch(&value, End(), 1));
        Check(!GuestWritable_nid_no_patch(End(), 1));
    }
    ~Pages() {
#ifdef _WIN32
        Check(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
#else
        Check(munmap(allocation, size * 2) == 0);
#endif
    }
    unsigned char* Start() { return allocation; }
    unsigned char* End() { return allocation + size; }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Check(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0);
#else
        Check(mprotect(allocation, size, PROT_READ) == 0);
#endif
        Check(!GuestWritable_nid_no_patch(Start(), sizeof(Links)));
    }
};

template <typename TCall>
static void Refuses(const char* function, TCall call) {
    const auto before = NotImplementedCalls_nid_no_patch();
    errno = 73;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find(function) != std::string_view::npos;
    }
    const int afterErrno = errno;
    Check(threw);
    Check(afterErrno == 73);
    Check(NotImplementedCalls_nid_no_patch() == before + 1);
}

static void InsqueGuards() {
    Pages pages;
    Links previous{};
    Links next{};
    Links element{&next, &previous};
    auto refuses = [&](void* elem, void* pred) {
        Refuses("insque_nid_postfix", [&] { insque_nid_postfix(elem, pred); });
    };
    refuses(pages.End(), nullptr);
    refuses(&element, pages.End());
    Check(element.forward == &next && element.back == &previous);
    void* overflow = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 3);
    refuses(overflow, nullptr);
    refuses(&element, overflow);
    for (void* badNext : {static_cast<void*>(pages.End()), static_cast<void*>(pages.End() - sizeof(void*)), overflow}) {
        previous.forward = badNext;
        refuses(&element, &previous);
        Check(previous.forward == badNext && previous.back == nullptr);
        Check(element.forward == &next && element.back == &previous);
    }
    void* prefix = &previous;
    auto* split = pages.End() - sizeof(void*);
    std::memcpy(split, &prefix, sizeof(prefix));
    refuses(split, nullptr);
    void* observed;
    std::memcpy(&observed, split, sizeof(observed));
    Check(observed == prefix);
    refuses(split, &previous);
    std::memcpy(&observed, split, sizeof(observed));
    Check(observed == prefix);

    auto* readonly = reinterpret_cast<Links*>(pages.Start());
    *readonly = Links{&next, &previous};
    pages.ReadOnly();
    refuses(readonly, nullptr);
    refuses(readonly, &previous);
    previous.forward = readonly;
    refuses(&element, &previous);
    Check(previous.forward == readonly && previous.back == nullptr);
    Check(element.forward == &next && element.back == &previous);
    Check(readonly->forward == &next && readonly->back == &previous);
    refuses(&element, readonly);
    Check(next.forward == nullptr && next.back == nullptr);
    Check(element.forward == &next && element.back == &previous);
}

static void RemqueGuards() {
    Pages pages;
    Links previous{};
    Links next{};
    Links element{&next, &previous};
    auto refuses = [&](void* elem) {
        Refuses("remque_nid_postfix", [&] { remque_nid_postfix(elem); });
    };
    refuses(pages.End());
    refuses(pages.End() - sizeof(void*));
    void* overflow = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 3);
    refuses(overflow);
    for (void* badNext : {static_cast<void*>(pages.End()), static_cast<void*>(pages.End() - sizeof(void*)), overflow}) {
        element.forward = badNext;
        refuses(&element);
        Check(previous.forward == nullptr && previous.back == nullptr);
        Check(element.forward == badNext && element.back == &previous);
    }
    element.forward = &next;
    for (void* badPrevious : {static_cast<void*>(pages.End()), overflow}) {
        element.back = badPrevious;
        refuses(&element);
        Check(next.forward == nullptr && next.back == nullptr);
        Check(element.forward == &next && element.back == badPrevious);
    }
    auto* readonly = reinterpret_cast<Links*>(pages.Start());
    *readonly = Links{&next, &previous};
    pages.ReadOnly();
    element = Links{readonly, &previous};
    refuses(&element);
    Check(previous.forward == nullptr && previous.back == nullptr);
    element = Links{&next, readonly};
    refuses(&element);
    Check(next.forward == nullptr && next.back == nullptr);
    Check(readonly->forward == &next && readonly->back == &previous);
    errno = 73;
    remque_nid_postfix(readonly);
    Check(errno == 73);
    Check(previous.forward == &next && next.back == &previous);
    Check(readonly->forward == &next && readonly->back == &previous);
}

static void BoundariesAndAliases() {
    Pages pages;
    auto* field = pages.End() - sizeof(void*);
    void* pointer = nullptr;
    std::memcpy(field, &pointer, sizeof(pointer));
    Links element{};
    errno = 73;
    insque_nid_postfix(&element, field);
    Check(errno == 73);
    std::memcpy(&pointer, field, sizeof(pointer));
    Check(pointer == &element);
    Check(element.forward == nullptr && element.back == field);
    remque_nid_postfix(&element);
    Check(errno == 73);
    std::memcpy(&pointer, field, sizeof(pointer));
    Check(pointer == nullptr);

    Links one{&one, &one};
    insque_nid_postfix(&one, &one);
    Check(one.forward == &one && one.back == &one);
    remque_nid_postfix(&one);
    Check(one.forward == &one && one.back == &one);
    Links two{&one, &one};
    insque_nid_postfix(&two, &one);
    Check(one.forward == &two && one.back == &two);
    Check(two.forward == &one && two.back == &one);
    remque_nid_postfix(&two);
    Check(one.forward == &one && one.back == &one);
    Check(two.forward == &one && two.back == &one);
    Check(errno == 73);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (argc != 2) return 2;
    const std::string_view mode(argv[1]);
    std::fprintf(stderr, "Queue pointer guards: %s\n", argv[1]);
    std::fflush(stderr);
    if (mode == "insque") InsqueGuards();
    else if (mode == "remque") RemqueGuards();
    else if (mode == "valid") BoundariesAndAliases();
    else return 2;
    return 0;
}

#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal getdst test failed: %s\n", message);
    std::abort();
}

template <typename F>
bool Throws(F&& body) {
    try {
        body();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

class Pages {
    char* allocation = nullptr;
    std::size_t size = 0;
    std::size_t readable = 0;
    std::size_t total = 0;
public:
    Pages(std::size_t count, bool guard) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        readable = size * count;
        total = size * (count + (guard ? 1 : 0));
        allocation = static_cast<char*>(VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr, "allocate pages");
        if (guard) {
            DWORD previous;
            Require(VirtualProtect(allocation + readable, size, PAGE_NOACCESS, &previous) != 0, "protect page");
        }
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0, "page size");
        size = static_cast<std::size_t>(page);
        readable = size * count;
        total = size * (count + (guard ? 1 : 0));
        void* mapped = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "allocate pages");
        allocation = static_cast<char*>(mapped);
        if (guard) Require(mprotect(allocation + readable, size, PROT_NONE) == 0, "protect page");
#endif
        std::memset(allocation, 0xa5, readable);
        if (guard) {
            char value = 0;
            Require(!GuestCopyIn_nid_no_patch(&value, allocation + readable, 1), "the guard page is unreadable");
        }
    }
    ~Pages() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0, "release pages");
#else
        Require(munmap(allocation, total) == 0, "release pages");
#endif
    }
    Pages(const Pages&) = delete;
    Pages& operator=(const Pages&) = delete;
    char* Begin() { return allocation; }
    char* End() { return allocation + readable; }
    std::size_t Bytes() { return readable; }
};

void WriteRules(char* destination, std::size_t bytes) {
    const char pattern[] = ":0101";
    for (std::size_t i = 0; i < bytes; ++i) destination[i] = pattern[i % 5];
}

}

int main() {
    Require(_Getdst_nid_postfix(nullptr) == nullptr, "null input returned non-null");

    const char* r = _Getdst_nid_postfix(":0101");
    Require(r != nullptr, "default rule returned null");
    const auto* b = reinterpret_cast<const unsigned char*>(r);
    Require(b[0] == 0, "byte 0 != 0");
    Require(b[1] == 0, "byte 1 != 0");
    Require(b[2] == 0, "byte 2 != 0");
    Require(b[3] == 0, "byte 3 != 0");
    Require(b[4] == 0, "byte 4 != 0");
    Require(b[5] == 0xff, "byte 5 != 0xff");
    std::free(const_cast<char*>(r));

    Require(Throws([] { _Getdst_nid_postfix(reinterpret_cast<const char*>(0x10)); }), "unreadable pointer did not throw");

    {
        Pages pages(1, true);
        char* const tail = pages.End() - 30;
        WriteRules(tail, 30);
        Require(Throws([tail] { _Getdst_nid_postfix(tail); }),
                "a rule string that runs off a readable page did not throw");
    }

    {
        Pages pages(32, false);
        WriteRules(pages.Begin(), pages.Bytes());
        char* const begin = pages.Begin();
        Require(Throws([begin] { _Getdst_nid_postfix(begin); }),
                "a rule string without a NUL within 0x10000 bytes did not throw");
    }

    return 0;
}

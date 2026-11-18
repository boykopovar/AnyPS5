#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace QueryOutputFixture {

inline void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "FreeType query output guard failed at line %u\n", location.line());
    std::abort();
}

class Pages {
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO information;
        GetSystemInfo(&information);
        page = information.dwPageSize;
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(memory != nullptr);
#else
        const long nativePage = sysconf(_SC_PAGESIZE);
        Check(nativePage > 0);
        page = static_cast<std::size_t>(nativePage);
        memory = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Check(memory != MAP_FAILED);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        Check(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Check(munmap(memory, page * 2) == 0);
#endif
    }

    Pages(const Pages&) = delete;
    Pages& operator=(const Pages&) = delete;

    template <typename TCall>
    void Reject(std::size_t width, TCall call) {
        Check(width > 1 && width < page);
        const std::array offsets{std::size_t{16}, page - width / 2, page + 16};
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            std::memset(memory, 0xA5, page * 2);
            const std::vector<unsigned char> before(memory, memory + page * 2);
            const std::size_t selected = index == 0 ? 0 : 1;
            Protect(selected, index == 0 ? 1 : 0);
            bool correct = false;
            try {
                correct = call(memory + offsets[index]);
            } catch (...) {
                Protect(selected, 3);
                throw;
            }
            Protect(selected, 3);
            Check(correct);
            Check(std::memcmp(memory, before.data(), before.size()) == 0);
        }
    }

    template <typename TCall>
    void Accept(std::size_t width, TCall call) {
        Check(width > 1 && width < page);
        std::memset(memory, 0xA5, page * 2);
        const std::vector<unsigned char> before(memory, memory + page * 2);
        Protect(1, 0);
        bool correct = false;
        try {
            correct = call(memory + page - width);
        } catch (...) {
            Protect(1, 3);
            throw;
        }
        Protect(1, 3);
        Check(correct);
        Check(std::memcmp(memory, before.data(), page - width) == 0);
        Check(std::memcmp(memory + page, before.data() + page, page) == 0);
    }

private:
    void Protect(std::size_t index, int protection) {
#ifdef _WIN32
        DWORD previous;
        const DWORD native = protection == 3 ? PAGE_READWRITE : protection == 1 ? PAGE_READONLY : PAGE_NOACCESS;
        Check(VirtualProtect(memory + page * index, page, native, &previous) != 0);
#else
        const int native = protection == 3 ? PROT_READ | PROT_WRITE : protection == 1 ? PROT_READ : PROT_NONE;
        Check(mprotect(memory + page * index, page, native) == 0);
#endif
    }

    std::size_t page = 0;
    unsigned char* memory = nullptr;
};

}

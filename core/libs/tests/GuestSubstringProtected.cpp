#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestSubstringProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

class Buffer {
    unsigned char* allocation;
    std::size_t page;
public:
    Buffer() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + page, page, PAGE_READWRITE, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        allocation = static_cast<unsigned char*>(mmap(nullptr, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + page, page, PROT_READ | PROT_WRITE) == 0);
#endif
        std::memset(allocation + page, 0xa5, page);
    }
    ~Buffer() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(allocation, page * 3) == 0);
#endif
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    unsigned char* At(std::size_t count) {
        Require(count <= page - 32);
        return allocation + page * 2 - (std::max)(count, std::size_t{1});
    }
};

int main() {
    Buffer hayBuffer, needleBuffer;
    for (std::size_t length = 0; length <= 160; ++length) {
        std::string repeating(length, 'a');
        if (!repeating.empty()) repeating.back() = 'b';
        const std::string high = std::string(length, static_cast<char>(0x80)) + static_cast<char>(0xff);
        for (const auto& haystack : {repeating, high}) {
            for (const auto& needle : {std::string{}, std::string("aab"), std::string("ba"), repeating,
                                      std::string("aaaaac"), std::string(3, static_cast<char>(0x80)) + static_cast<char>(0xff),
                                      haystack + "extra"}) {
                auto* hay = reinterpret_cast<char*>(hayBuffer.At(haystack.size() + 1));
                auto* query = reinterpret_cast<char*>(needleBuffer.At(needle.size() + 1));
                std::memcpy(hay, haystack.c_str(), haystack.size() + 1);
                std::memcpy(query, needle.c_str(), needle.size() + 1);
                const auto position = haystack.find(needle);
                Require(strstr_nid_postfix(hay, query) == (position == std::string::npos ? nullptr : hay + position));
                Require(std::memcmp(hay, haystack.c_str(), haystack.size() + 1) == 0);
                Require(std::memcmp(query, needle.c_str(), needle.size() + 1) == 0);
            }
        }
    }
}

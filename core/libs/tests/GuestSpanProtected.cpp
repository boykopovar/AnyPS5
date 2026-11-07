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
        std::fputs("GuestSpanProtected check failed\n", stderr);
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
    Buffer textBuffer, setBuffer;
    for (unsigned selected = 0; selected < 256; ++selected) {
        std::string members;
        std::array<bool, 256> present{};
        for (unsigned value = 1; value < 256; ++value) {
            if ((value + selected) % 7 == 0 || value == selected) {
                members.push_back(static_cast<char>(value));
                present[value] = true;
            }
        }
        if (selected == 0) { members.clear(); present.fill(false); }
        auto* set = reinterpret_cast<char*>(setBuffer.At(members.size() + 1));
        std::memcpy(set, members.c_str(), members.size() + 1);
        for (std::size_t length : std::array<std::size_t, 9>{0, 1, 7, 15, 16, 31, 32, 63, 127}) {
            std::string input;
            for (std::size_t i = 0; i < length; ++i) input.push_back(static_cast<char>(1 + (i * 37 + selected) % 255));
            auto* text = reinterpret_cast<char*>(textBuffer.At(length + 1));
            std::memcpy(text, input.c_str(), length + 1);
            std::size_t accepted = 0, rejected = 0;
            while (accepted < length && present[static_cast<unsigned char>(input[accepted])]) ++accepted;
            while (rejected < length && !present[static_cast<unsigned char>(input[rejected])]) ++rejected;
            Require(strspn_nid_postfix(text, set) == accepted);
            Require(strcspn_nid_postfix(text, set) == rejected);
            Require(std::memcmp(text, input.c_str(), length + 1) == 0);
            Require(std::memcmp(set, members.c_str(), members.size() + 1) == 0);
        }
    }
}

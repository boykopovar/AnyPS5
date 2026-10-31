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
        std::fputs("GuestWideTokenBoundary check failed\n", stderr);
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
    template<class T = unsigned char>
    T* At(std::size_t count) {
        Require(count <= (page - 32) / sizeof(T));
        return reinterpret_cast<T*>(allocation + page * 2 - (std::max)(count, std::size_t{1}) * sizeof(T));
    }
};

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    Buffer input, delimiters, other;
    auto* set = delimiters.At<char16_t>(3);
    set[0] = 0x100; set[1] = 0xd800; set[2] = 0;
    const std::u16string separator(set, 2);
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char16_t>(length + 1);
        const std::array<char16_t, 7> values{0x100, 1, 0x8000, 0xd800, 0xffff, 0x100, 0xdc00};
        for (std::size_t i = 0; i < length; ++i) source[i] = values[(i + length) % values.size()];
        source[length] = 0;
        const std::u16string saved(source, length + 1);
        std::vector<std::u16string> expected;
        std::u16string field;
        for (std::size_t i = 0; i < length; ++i) {
            if (separator.find(saved[i]) != std::u16string::npos) {
                if (!field.empty()) { expected.push_back(field); field.clear(); }
            } else field.push_back(saved[i]);
        }
        if (!field.empty()) expected.push_back(field);
        auto* alternate = other.At<char16_t>(4);
        alternate[0] = 7; alternate[1] = 0x100; alternate[2] = 9; alternate[3] = 0;
        char16_t* state = nullptr;
        char16_t* otherState = nullptr;
        Require(wcstok_nid_postfix(alternate, set, &otherState) == alternate && alternate[0] == 7 && alternate[1] == 0);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto* token = wcstok_nid_postfix(i == 0 ? source : nullptr, set, &state);
            Require(token != nullptr && std::u16string(token) == expected[i]);
        }
        Require(wcstok_nid_postfix(expected.empty() ? source : nullptr, set, &state) == nullptr);
        Require(wcstok_nid_postfix(nullptr, set, &state) == nullptr);
        auto* last = wcstok_nid_postfix(nullptr, set, &otherState);
        Require(last == alternate + 2 && last[0] == 9 && last[1] == 0);
        for (std::size_t i = 0; i <= length; ++i) if (separator.find(saved[i]) == std::u16string::npos) Require(source[i] == saved[i]);
        Require(set[0] == 0x100 && set[1] == 0xd800 && set[2] == 0);
    }
}

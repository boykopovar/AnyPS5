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
        std::fputs("GuestTokenizerProtected check failed\n", stderr);
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
    Buffer data, delimiters;
    for (unsigned value : {1u, 44u, 127u, 128u, 255u}) {
        const char delimiter = static_cast<char>(value);
        std::string separators(1, delimiter);
        separators += ':';
        auto* set = reinterpret_cast<char*>(delimiters.At(3));
        std::memcpy(set, separators.c_str(), 3);
        const char payload = value == 'x' ? 'q' : 'x';
        for (std::size_t repeats = 0; repeats <= 32; ++repeats) {
            std::string input(1, delimiter);
            input += std::string(repeats, payload);
            input += ":";
            input += delimiter;
            input += "tail:";
            std::vector<std::string> fields;
            std::size_t begin = 0;
            for (std::size_t i = 0; i <= input.size(); ++i) {
                if (i == input.size() || input[i] == delimiter || input[i] == ':') {
                    fields.push_back(input.substr(begin, i - begin));
                    begin = i + 1;
                }
            }
            auto* text = reinterpret_cast<char*>(data.At(input.size() + 1));
            std::memcpy(text, input.c_str(), input.size() + 1);
            char* cursor = text;
            for (const auto& field : fields) {
                char* token = strsep_nid_postfix(&cursor, set);
                Require(token != nullptr && std::string(token) == field);
            }
            Require(strsep_nid_postfix(&cursor, set) == nullptr);
            std::memcpy(text, input.c_str(), input.size() + 1);
            char* state = nullptr;
            char* token = strtok_r_nid_postfix(text, set, &state);
            for (const auto& field : fields) {
                if (field.empty()) continue;
                Require(token != nullptr && std::string(token) == field);
                token = strtok_r_nid_postfix(nullptr, set, &state);
            }
            Require(token == nullptr);
            for (std::size_t i = 0; i < input.size(); ++i) {
                if (input[i] != delimiter && input[i] != ':') Require(text[i] == input[i]);
            }
            Require(std::memcmp(set, separators.c_str(), 3) == 0);
        }
    }
}

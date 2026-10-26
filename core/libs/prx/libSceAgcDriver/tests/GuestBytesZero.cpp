#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

bool scan(const unsigned char* begin, std::size_t bytes) {
    return AgcDriver::Graphics::GuestBytesZero(reinterpret_cast<std::uintptr_t>(begin), reinterpret_cast<std::uintptr_t>(begin + bytes));
}

void smallRanges() {
    alignas(64) std::array<unsigned char, 256> memory;
    for (std::size_t offset = 1; offset <= 64; ++offset) {
        for (std::size_t bytes = 0; bytes <= 129; ++bytes) {
            memory.fill(0);
            auto* begin = memory.data() + offset;
            begin[-1] = begin[bytes] = 0xff;
            const auto before = memory;
            check(scan(begin, bytes), "a zero range includes a neighboring nonzero byte");
            for (std::size_t position = 0; position < bytes; ++position) {
                for (unsigned bit = 0; bit < 8; ++bit) {
                    begin[position] = static_cast<unsigned char>(1u << bit);
                    check(!scan(begin, bytes), "a nonzero bit was missed at a small-range alignment or tail");
                }
                begin[position] = 0;
            }
            check(memory == before, "a scan changed its input or neighboring bytes");
        }
    }
}

void largeRanges() {
    std::vector<unsigned char> memory(65536 + 65);
    for (std::size_t offset = 1; offset <= 64; ++offset) {
        for (const auto bytes : {255u, 256u, 257u, 4095u, 4096u, 4097u, 65535u, 65536u}) {
            std::fill(memory.begin(), memory.end(), 0);
            auto* begin = memory.data() + offset;
            begin[-1] = begin[bytes] = 0xff;
            check(scan(begin, bytes), "a large zero range includes a neighboring nonzero byte");
            for (const auto position : {0u, 7u, 8u, 15u, 16u, 23u, 24u, 31u, 32u, bytes / 2, bytes - 1}) {
                for (unsigned bit = 0; bit < 8; ++bit) {
                    begin[position] = static_cast<unsigned char>(1u << bit);
                    check(!scan(begin, bytes), "a nonzero bit was missed in a large range");
                }
                begin[position] = 0;
            }
        }
    }
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, 3 * bytes, MEM_RESERVE, PAGE_NOACCESS));
        check(allocation != nullptr, "cannot reserve the guard fixture");
        if (VirtualAlloc(allocation + bytes, bytes, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
            VirtualFree(allocation, 0, MEM_RELEASE);
            throw std::runtime_error("cannot commit the readable page");
        }
#else
        bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        auto* mapped = mmap(nullptr, 3 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        check(mapped != MAP_FAILED, "cannot map the guard fixture");
        allocation = static_cast<unsigned char*>(mapped);
        if (mprotect(allocation + bytes, bytes, PROT_READ | PROT_WRITE) != 0) {
            munmap(allocation, 3 * bytes);
            throw std::runtime_error("cannot make the middle page readable");
        }
#endif
        std::memset(Data(), 0, bytes);
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, 3 * bytes);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
    unsigned char* Data() const { return allocation + bytes; }
    std::size_t Size() const { return bytes; }

private:
    unsigned char* allocation = nullptr;
    std::size_t bytes = 0;
};

void guardPages() {
    GuardedPage page;
    for (std::size_t bytes = 0; bytes <= std::min<std::size_t>(page.Size(), 1024); ++bytes) {
        for (auto* begin : {page.Data(), page.Data() + page.Size() - bytes}) {
            check(scan(begin, bytes), "a guarded zero range was classified as nonzero");
            if (bytes == 0) continue;
            begin[bytes - 1] = 0x80;
            check(!scan(begin, bytes), "the last byte before a guard page was missed");
            begin[bytes - 1] = 0;
        }
    }
    for (std::size_t readable = 1; readable <= 64; ++readable) {
        auto* begin = page.Data() + page.Size() - readable;
        for (std::size_t position = 0; position < readable; ++position) {
            begin[position] = 0x80;
            check(!scan(begin, readable + 32), "the scan read past a nonzero word into a protected page");
            begin[position] = 0;
        }
    }
}

}

int main() {
    try {
        smallRanges();
        largeRanges();
        guardPages();
        std::cout << "GuestBytesZero ranges, nonzero bits and guard pages passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using SetSpeed = int (APS5_VABI*)(void*, std::uint32_t);

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "cfsetspeed: %s\n", message);
        std::abort();
    }
}

std::uint32_t Read(const unsigned char* data) {
    std::uint32_t result;
    std::memcpy(&result, data, sizeof(result));
    return result;
}

void Check(SetSpeed setSpeed, unsigned char* attributes, std::uint32_t speed) {
    std::array<unsigned char, 44> before;
    std::memcpy(before.data(), attributes, before.size());
    *__error_nid_postfix() = 123;
    Require(setSpeed(attributes, speed) == 0, "valid call failed");
    Require(*__error_nid_postfix() == 123, "successful call changed errno");
    Require(Read(attributes + 36) == speed, "incorrect input speed");
    Require(Read(attributes + 40) == speed, "incorrect output speed");
    Require(std::memcmp(before.data(), attributes, 36) == 0, "flags or control characters changed");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected the kernel library path");
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "could not load libkernel");
    const auto setSpeed = reinterpret_cast<SetSpeed>(GetProcAddress(library, "cfsetspeed_nid_postfix"));
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "could not allocate boundary fixture");
    DWORD previous = 0;
    Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0, "could not protect boundary fixture");
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(library != nullptr, "could not load libkernel");
    const auto setSpeed = reinterpret_cast<SetSpeed>(dlsym(library, "cfsetspeed_nid_postfix"));
    const long pageSize = sysconf(_SC_PAGESIZE);
    Require(pageSize > 0, "could not query page size");
    const auto page = static_cast<std::size_t>(pageSize);
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED, "could not allocate boundary fixture");
    Require(mprotect(memory + page, page, PROT_NONE) == 0, "could not protect boundary fixture");
#endif
    Require(setSpeed != nullptr, "export is missing");
    for (const auto speed : {0u, 1u, 50u, 9600u, 115200u, 0x12345678u, 0x80000000u, 0xffffffffu}) {
        alignas(8) std::array<unsigned char, 60> bytes;
        bytes.fill(0xa5);
        Check(setSpeed, bytes.data() + 8, speed);
        for (std::size_t i = 0; i < 8; ++i) {
            Require(bytes[i] == 0xa5 && bytes[52 + i] == 0xa5, "write escaped the termios object");
        }
        auto* boundary = memory + page - 44;
        std::memset(boundary - 8, 0x5a, 52);
        Check(setSpeed, boundary, speed);
        for (std::size_t i = 0; i < 8; ++i) Require(boundary[-8 + static_cast<int>(i)] == 0x5a, "write preceded the termios object");
    }
    bool threw = false;
    try {
        static_cast<void>(setSpeed(nullptr, 9600));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null pointer must be rejected explicitly");
#ifdef _WIN32
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0, "could not free boundary fixture");
    FreeLibrary(library);
#else
    Require(munmap(memory, 2 * page) == 0, "could not free boundary fixture");
    dlclose(library);
#endif
    return 0;
}

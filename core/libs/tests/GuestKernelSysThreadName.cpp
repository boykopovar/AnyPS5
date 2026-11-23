#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr std::int32_t SceEsrch = static_cast<std::int32_t>(0x80020003u);
constexpr std::int32_t SceEfault = static_cast<std::int32_t>(0x8002000eu);
constexpr std::int32_t SceEnametoolong = static_cast<std::int32_t>(0x8002003fu);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "thread name check failed at line %u\n", location.line());
        std::abort();
    }
}

std::atomic<std::uint32_t> workerId{0};
std::atomic<bool> release{false};

void* APS5_VABI Worker(void*) {
    workerId.store(static_cast<std::uint32_t>(scePthreadGetthreadid()));
    while (!release.load()) scePthreadYield();
    return nullptr;
}

} // namespace

int main() {
    Pthread worker{};
    Require(scePthreadCreate(&worker, nullptr, Worker, nullptr, "name-test") == 0);
    while (workerId.load() == 0) scePthreadYield();

    char name[64];
    std::memset(name, 0xa5, sizeof(name));
    Require(sceKernelGetThreadName(workerId.load(), name) == 0);
    Require(std::strcmp(name, "name-test") == 0);
    Require(static_cast<unsigned char>(name[10]) == 0xa5);

    Require(scePthreadRename(worker, "abcdefghijklmnopqrstuvwxyz01234") == 0);
    Require(sceKernelGetThreadName(workerId.load(), name) == 0);
    Require(std::strlen(name) == 31);
    for (const char* tooLong : {"abcdefghijklmnopqrstuvwxyz012345", "abcdefghijklmnopqrstuvwxyz0123456789"}) {
        Require(scePthreadRename(worker, tooLong) == SceEnametoolong);
        Require(sceKernelGetThreadName(workerId.load(), name) == 0);
        Require(std::strcmp(name, "abcdefghijklmnopqrstuvwxyz01234") == 0);
    }

    Require(scePthreadRename(worker, reinterpret_cast<const char*>(0x1000)) == SceEfault);
    Require(sceKernelGetThreadName(workerId.load(), name) == 0);
    Require(std::strcmp(name, "abcdefghijklmnopqrstuvwxyz01234") == 0);

    Require(scePthreadRename(worker, "") == 0);
    std::memset(name, 0xa5, sizeof(name));
    Require(sceKernelGetThreadName(workerId.load(), name) == 0);
    Require(name[0] == '\0' && static_cast<unsigned char>(name[1]) == 0xa5);

    Pthread invalid = reinterpret_cast<Pthread>(0x12345);
    for (const char* bad : {"ok", "abcdefghijklmnopqrstuvwxyz0123456789", reinterpret_cast<const char*>(0x1000)}) {
        Require(scePthreadRename(invalid, bad) == SceEsrch);
    }

    Require(sceKernelGetThreadName(workerId.load(), nullptr) == SceEfault);
    Require(sceKernelGetThreadName(workerId.load(), reinterpret_cast<char*>(1)) == SceEfault);
    Require(sceKernelGetThreadName(99999999, name) == SceEsrch);
    Require(sceKernelGetThreadName(0, name) == SceEsrch);
    Require(sceKernelGetThreadName(99999999, nullptr) == SceEsrch);
    Require(sceKernelGetThreadName(99999999, reinterpret_cast<char*>(1)) == SceEsrch);

    release.store(true);
    Require(scePthreadJoin(worker, nullptr) == 0);
    Require(sceKernelGetThreadName(workerId.load(), name) == SceEsrch);

    char self[64];
    Require(sceKernelGetThreadName(static_cast<std::uint32_t>(scePthreadGetthreadid()), self) == 0);
    return 0;
}

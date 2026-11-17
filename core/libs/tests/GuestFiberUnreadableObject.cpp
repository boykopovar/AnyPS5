#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);

void Require(bool condition) {
    if (!condition) std::abort();
}

void APS5_VABI Entry(std::uint64_t, std::uint64_t) {
    std::abort();
}

void* ReadOnlyPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    void* page = mmap(nullptr, static_cast<std::size_t>(size), PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

}

int main() {
    auto* const unmapped = reinterpret_cast<FiberObject*>(0x10);
    auto* const readOnly = static_cast<FiberObject*>(ReadOnlyPage());
    alignas(16) static std::uint8_t stack[16384];
    std::uint64_t value = 0;

    errno = 71;
    Require(sceFiberFinalize(unmapped) == FiberErrorInvalid);
    Require(sceFiberRun_nid_postfix(unmapped, 0, &value) == FiberErrorInvalid);
    Require(sceFiberSwitch(unmapped, 0, &value) == FiberErrorInvalid);
    Require(sceFiberRename(unmapped, "name") == FiberErrorInvalid);
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(unmapped, &info) == FiberErrorInvalid);
    Require(errno == 71);

    Require(_sceFiberInitializeImpl_nid_postfix(unmapped, "fiber", Entry, 0, stack, sizeof(stack), nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeImpl_nid_postfix(readOnly, "fiber", Entry, 0, stack, sizeof(stack), nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeWithInternalOptionImpl(readOnly, "fiber", Entry, 0, nullptr, 0, nullptr, 0, 0) == FiberErrorInvalid);
    Require(sceFiberFinalize(readOnly) == FiberErrorInvalid);
    Require(errno == 71);
    return 0;
}

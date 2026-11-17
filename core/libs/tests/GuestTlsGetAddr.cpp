#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#else
#include <link.h>
#include <sys/mman.h>
#endif

#if defined(_WIN32)
extern "C" ULONG _tls_index;

namespace {

using Cell = char;
constexpr Cell Initial = 0;

KernelTlsIndex CellIndex() { return {reinterpret_cast<std::uintptr_t>(&_tls_index), 0}; }

Cell* ThreadCell() {
    auto* blocks = *reinterpret_cast<char***>(reinterpret_cast<char*>(NtCurrentTeb()) + 0x58);
    return blocks[_tls_index];
}

}
#else
namespace {

using Cell = std::uint32_t;
constexpr Cell Initial = 0x5ca1ab1e;
thread_local Cell Value = Initial;

struct Module {
    std::size_t id = 0;
    void* data = nullptr;
};

Module MainModule() {
    Module module;
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* out) {
        auto& found = *static_cast<Module*>(out);
        found = {info->dlpi_tls_modid, info->dlpi_tls_data};
        return 1;
    }, &module);
    return module;
}

KernelTlsIndex CellIndex() {
    const auto module = MainModule();
    return {module.id, static_cast<std::uint64_t>(reinterpret_cast<char*>(&Value) - static_cast<char*>(module.data))};
}

Cell* ThreadCell() { return &Value; }

}
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    const auto index = CellIndex();
    auto* own = static_cast<Cell*>(__tls_get_addr_nid_postfix(&index));
    Require(own == ThreadCell() && *own == Initial);
    *own = 7;
    Cell* other = nullptr;
    Cell seen = 1;
    std::thread([&] {
        other = static_cast<Cell*>(__tls_get_addr_nid_postfix(&index));
        Require(other == ThreadCell());
        seen = *other;
        *other = 9;
    }).join();
    Require(other != own && seen == Initial && *own == 7);
    *own = Initial;
    bool threw = false;
    try { __tls_get_addr_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

#if defined(_WIN32)
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(memory != nullptr);
#else
    constexpr std::size_t Page = 0x4000;
    auto* memory = static_cast<char*>(mmap(nullptr, Page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED);
#endif
    KernelTlsIndex guest{};
    std::memcpy(memory, &guest, sizeof(guest));
#if defined(_WIN32)
    DWORD old = 0;
    Require(VirtualProtect(memory, 0x1000, PAGE_NOACCESS, &old) != FALSE);
#else
    Require(mprotect(memory, Page, PROT_NONE) == 0);
#endif
    const int savedErrno = errno;
    bool unreadableThrew = false;
    try { __tls_get_addr_nid_postfix(reinterpret_cast<const KernelTlsIndex*>(memory)); } catch (const std::invalid_argument&) { unreadableThrew = true; }
    Require(unreadableThrew);
    Require(errno == savedErrno);
}

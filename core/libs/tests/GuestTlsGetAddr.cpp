#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#else
#include <link.h>
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

template <typename TCall>
bool RejectsInvalid(TCall call) {
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument&) {
        return errno == 71;
    }
    return false;
}

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
    Require(RejectsInvalid([&] { __tls_get_addr_nid_postfix(nullptr); }));
    Require(RejectsInvalid([&] {
        __tls_get_addr_nid_postfix(reinterpret_cast<const KernelTlsIndex*>(0x10));
    }));
#if defined(_WIN32)
    Require(RejectsInvalid([&] {
        const KernelTlsIndex unreadableModule{0x10, 0};
        __tls_get_addr_nid_postfix(&unreadableModule);
    }));
#endif
}

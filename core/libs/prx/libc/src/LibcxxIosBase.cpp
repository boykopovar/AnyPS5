#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t StateOffset = 32;
constexpr std::uint32_t BadBit = 0x1;

struct GuestIosState {
    std::uint32_t state;
    std::uint32_t exceptions;
};

}

extern "C" void APS5_VABI _ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv_nid_postfix(void* self) {
    if (self == nullptr) throw std::invalid_argument("libc++ ios_base: null object");
    auto* address = static_cast<std::byte*>(self) + StateOffset;
    GuestIosState current{};
    if (!GuestCopyIn_nid_no_patch(&current, address, sizeof(current))) throw std::invalid_argument("libc++ ios_base: unreadable object");
    current.state |= BadBit;
    if (!GuestCopyOut_nid_no_patch(address, &current.state, sizeof(current.state))) throw std::invalid_argument("libc++ ios_base: unwritable object");
    if ((current.exceptions & BadBit) != 0) __cxa_rethrow_nid_postfix();
}

#include "execution/TargetAddressWindow.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    constexpr std::size_t BlockBytes = 65536u;
    if (AgcDriver::Tests::TargetAddressWindowPair(0x1fffe0000ull, BlockBytes) != std::array<std::size_t, 2>{0u, 1u}) return 1;
    if (AgcDriver::Tests::TargetAddressWindowPair(0x1ffff0000ull, BlockBytes) != std::array<std::size_t, 2>{1u, 2u}) return 1;
    if (AgcDriver::Tests::TargetAddressWindowPair(0x300000000ull, BlockBytes) != std::array<std::size_t, 2>{0u, 1u}) return 1;
    try {
        AgcDriver::Tests::TargetAddressWindowPair(0x1ffff0000ull, static_cast<std::size_t>(0x100000000ull));
    } catch (const std::runtime_error&) {
        std::cout << "target address window test passed\n";
        return 0;
    }
    return 1;
}

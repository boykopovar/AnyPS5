#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

std::array<unsigned char, 16> ValueHolding(const std::uint64_t* slot) {
    std::array<unsigned char, 16> object{};
    std::memset(object.data(), 0x5A, object.size());
    std::memcpy(object.data(), &slot, sizeof(slot));
    return object;
}

} // namespace

int main() {
    std::array<unsigned char, 16> empty{};
    std::memset(empty.data(), 0x5A, empty.size());
    _ZN10Deprecated11ScriptValueC1Ev(empty.data());
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(empty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(empty.data()) == 0);

    for (const std::uint64_t encoded : {std::uint64_t{0x0002000000000007}, std::uint64_t{0xFFFE000000000001}, std::uint64_t{1}}) {
        std::uint64_t slot = encoded;
        const auto object = ValueHolding(&slot);
        Require(!_ZNK10Deprecated11ScriptValue10hasNoValueEv(object.data()));
        Require(_ZNK10Deprecated11ScriptValue7jsValueEv(object.data()) == encoded);
        Require(slot == encoded);
        for (std::size_t i = sizeof(&slot); i < object.size(); ++i) Require(object[i] == 0x5A);
    }

    std::uint64_t emptySlot = 0;
    const auto holdingEmpty = ValueHolding(&emptySlot);
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(holdingEmpty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(holdingEmpty.data()) == 0);
}

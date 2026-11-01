#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t Sentinel = -1;

bool IsLead(std::int32_t unit) {
    return (static_cast<std::uint32_t>(unit) & 0xfffffc00u) == 0xd800u;
}

bool IsTrail(std::int32_t unit) {
    return (static_cast<std::uint32_t>(unit) & 0xfffffc00u) == 0xdc00u;
}

template<typename TCallback>
TCallback Require(TCallback callback) {
    if (!callback) throw std::invalid_argument("u_strCompareIter: null iterator callback");
    return callback;
}

std::int32_t Next(UCharIterator* iterator) {
    return Require(iterator->next)(iterator);
}

bool InPair(UCharIterator* iterator, std::int32_t unit) {
    if (unit <= 0xdbff) return IsTrail(Require(iterator->current)(iterator));
    if (!IsTrail(unit)) return false;
    const auto previous = Require(iterator->previous);
    previous(iterator);
    return IsLead(Require(iterator->previous)(iterator));
}

std::int32_t Order(UCharIterator* iterator, std::int32_t unit) {
    return InPair(iterator, unit) ? unit : unit - 0x2800;
}

}

extern "C" {

std::int32_t APS5_VABI u_strCompareIter_nid_postfix(UCharIterator* first, UCharIterator* second, std::uint8_t codePointOrder) {
    if (!first || !second || first == second) return 0;
    Require(first->move)(first, 0, UITER_START);
    Require(second->move)(second, 0, UITER_START);
    std::int32_t left;
    std::int32_t right;
    for (;;) {
        left = Next(first);
        right = Next(second);
        if (left != right) break;
        if (left == Sentinel) return 0;
    }
    if (left >= 0xd800 && right >= 0xd800 && codePointOrder) {
        left = Order(first, left);
        right = Order(second, right);
    }
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}

}

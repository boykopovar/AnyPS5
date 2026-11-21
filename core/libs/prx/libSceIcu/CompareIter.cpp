#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
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

struct Iterator {
    UCharIterator* guest;
    UCharIterator callbacks;
};

Iterator Load(UCharIterator* guest) {
    Iterator iterator{guest, {}};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&iterator.callbacks, guest, sizeof(iterator.callbacks));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument("u_strCompareIter: unreadable iterator");
    return iterator;
}

std::int32_t Next(Iterator& iterator) {
    return Require(iterator.callbacks.next)(iterator.guest);
}

bool InPair(Iterator& iterator, std::int32_t unit) {
    if (unit <= 0xdbff) return IsTrail(Require(iterator.callbacks.current)(iterator.guest));
    if (!IsTrail(unit)) return false;
    const auto previous = Require(iterator.callbacks.previous);
    previous(iterator.guest);
    return IsLead(Require(iterator.callbacks.previous)(iterator.guest));
}

std::int32_t Order(Iterator& iterator, std::int32_t unit) {
    return InPair(iterator, unit) ? unit : unit - 0x2800;
}

}

extern "C" {

std::int32_t APS5_VABI u_strCompareIter_nid_postfix(UCharIterator* guestFirst, UCharIterator* guestSecond, std::uint8_t codePointOrder) {
    if (!guestFirst || !guestSecond || guestFirst == guestSecond) return 0;
    auto first = Load(guestFirst);
    auto second = Load(guestSecond);
    Require(first.callbacks.move)(guestFirst, 0, UITER_START);
    Require(second.callbacks.move)(guestSecond, 0, UITER_START);
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

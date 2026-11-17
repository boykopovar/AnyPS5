#include "prx/libSceLibcInternal/include/TimeGetOrder.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uintptr_t DateOrderOffset = 0x28;

std::int32_t ReadDateOrder_nid_no_patch(const void* self) {
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    if (address > std::numeric_limits<std::uintptr_t>::max() - DateOrderOffset)
        throw std::invalid_argument("time_get::do_date_order: invalid facet address");
    std::int32_t order = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&order, reinterpret_cast<const void*>(address + DateOrderOffset), sizeof(order));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("time_get::do_date_order: unreadable date order");
    return order;
}

}

extern "C" {

std::int32_t APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13do_date_orderEv_nid_postfix(const void* self) {
    return ReadDateOrder_nid_no_patch(self);
}

std::int32_t APS5_VABI _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE13do_date_orderEv_nid_postfix(const void* self) {
    return ReadDateOrder_nid_no_patch(self);
}

}

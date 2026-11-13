#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::uint8_t ByteExtremum(const void* first, const void* last, bool minimum) {
    if (!first || !last || reinterpret_cast<std::uintptr_t>(last) <= reinterpret_cast<std::uintptr_t>(first)) APS5_INVALID_ARG_EX;
    auto* current = static_cast<const std::uint8_t*>(first);
    const auto* end = static_cast<const std::uint8_t*>(last);
    std::uint8_t result = *current++;
    for (; current != end; ++current) {
        if (minimum ? *current < result : *current > result) result = *current;
    }
    return result;
}

}

extern "C" {

int APS5_VABI __std_atomic_notify_multi_direct_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::uint8_t APS5_VABI __std_max_1u_nid_postfix(const void* first, const void* last) {
    return ByteExtremum(first, last, false);
}

std::uint8_t APS5_VABI __std_min_1u_nid_postfix(const void* first, const void* last) {
    return ByteExtremum(first, last, true);
}

int APS5_VABI __truncdfhf2_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcInternalPrimitiveMallocStatsFast_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __std_atomic_wait_direct_8_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceLibcInternalPrimitiveMallocStats_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __std_atomic_wait_direct_16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}

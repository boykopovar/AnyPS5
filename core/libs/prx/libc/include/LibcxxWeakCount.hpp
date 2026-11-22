#pragma once

#include "general/VabiMacros.hpp"
#include <cstdint>

struct LibcxxWeakCount {
    const void* vtable;
    std::uint64_t sharedOwners;
    std::int64_t weakOwners;
};

extern "C" {
LibcxxWeakCount* APS5_VABI _ZNSt3__119__shared_weak_count4lockEv_nid_postfix(LibcxxWeakCount* self);
void APS5_VABI _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(LibcxxWeakCount* self);
}

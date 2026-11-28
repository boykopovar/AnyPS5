#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include <atomic>
#include <cstdint>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZNSt6locale5facet7_IncrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("std::locale::facet::_Incref: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI _ZNSt6locale5facet7_DecrefEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("std::locale::facet::_Decref: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

}

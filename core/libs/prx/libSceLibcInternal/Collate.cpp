#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <stdexcept>
#include <string>

namespace {

const GuestLocale::CollateVtable& BaseCollate() {
    return *reinterpret_cast<const GuestLocale::CollateVtable*>(&_ZTVSt7collateIcE_nid_postfix[2]);
}

const GuestLocale::CollateVtable& FacetCollate(const GuestLocale::CollateFacet* self, const char* function) {
    if (self == nullptr || self->base.vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const GuestLocale::CollateVtable*>(self->base.vtable);
}

}

extern "C" {

int APS5_VABI _ZNKSt7collateIcE10do_compareEPKcS2_S2_S2_(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2) {
    return BaseCollate().compare(self, first1, last1, first2, last2);
}

GuestLocale::String* APS5_VABI _ZNKSt7collateIcE12do_transformEPKcS2_(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return BaseCollate().transform(result, self, first, last);
}

std::int64_t APS5_VABI _ZNKSt7collateIcE7do_hashEPKcS2_(const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return BaseCollate().hash(self, first, last);
}

int APS5_VABI _ZNKSt7collateIcE7compareEPKcS2_S2_S2_(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2) {
    return FacetCollate(self, "collate::compare").compare(self, first1, last1, first2, last2);
}

std::int64_t APS5_VABI _ZNKSt7collateIcE4hashEPKcS2_(const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return FacetCollate(self, "collate::hash").hash(self, first, last);
}

}

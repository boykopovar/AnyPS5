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

int APS5_VABI sceLibcInternalCollateDoCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2) {
    return BaseCollate().compare(self, first1, last1, first2, last2);
}
APS5_EXPORT("1eEXfeW6wrI", sceLibcInternalCollateDoCompare_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalCollateDoTransform_nid_no_patch(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return BaseCollate().transform(result, self, first, last);
}
APS5_EXPORT("gYlF567r3-A", sceLibcInternalCollateDoTransform_nid_no_patch);

std::int64_t APS5_VABI sceLibcInternalCollateDoHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return BaseCollate().hash(self, first, last);
}
APS5_EXPORT("GSAXi4F1SlM", sceLibcInternalCollateDoHash_nid_no_patch);

int APS5_VABI sceLibcInternalCollateCompare_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2) {
    return FacetCollate(self, "collate::compare").compare(self, first1, last1, first2, last2);
}
APS5_EXPORT("Q-8lQCGMj4g", sceLibcInternalCollateCompare_nid_no_patch);

std::int64_t APS5_VABI sceLibcInternalCollateHash_nid_no_patch(const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    return FacetCollate(self, "collate::hash").hash(self, first, last);
}
APS5_EXPORT("6vYXzFD-mrk", sceLibcInternalCollateHash_nid_no_patch);

}

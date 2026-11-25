#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
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

struct ScopedLocinfo {
    GuestLocale::LocinfoStorage storage{};
    bool active = false;

    explicit ScopedLocinfo(const char* localeName) {
        _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, localeName);
        active = true;
    }

    ~ScopedLocinfo() {
        if (active) _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
    }

    ScopedLocinfo(const ScopedLocinfo&) = delete;
    ScopedLocinfo& operator=(const ScopedLocinfo&) = delete;
};

void InitCollateFacet(GuestLocale::CollateFacet* self, std::size_t references) {
    if (self == nullptr) throw std::invalid_argument("collate<char>::collate: null object");
    self->base.vtable = &BaseCollate().facet;
    self->base.references = static_cast<std::uint32_t>(references);
    self->base.reserved = static_cast<std::uint32_t>(static_cast<std::uint64_t>(references) >> 32);
    self->collation = nullptr;
    self->wideCollation = nullptr;
}

void ConstructNamedCollate(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    if (self == nullptr) throw std::invalid_argument("collate<char>::collate: null object");
    ScopedLocinfo locinfo(localeName);
    InitCollateFacet(self, references);
}

}

extern "C" {

void APS5_VABI _ZNSt7collateIcEC1Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, "C", references);
}

void APS5_VABI _ZNSt7collateIcEC1EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, localeName, references);
}

void APS5_VABI _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    if (self == nullptr) throw std::invalid_argument("collate<char>::collate: null object");
    if (locinfo == nullptr) throw std::invalid_argument("collate<char>::collate: null _Locinfo");
    InitCollateFacet(self, references);
}

void APS5_VABI _ZNSt7collateIcEC2EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructNamedCollate(self, localeName, references);
}

void APS5_VABI _ZNSt7collateIcEC2Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructNamedCollate(self, "C", references);
}

void APS5_VABI _ZNSt7collateIcEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(self, locinfo, references);
}

void APS5_VABI _ZNSt7collateIcED0Ev_nid_postfix(GuestLocale::CollateFacet* self) {
    if (self == nullptr) throw std::invalid_argument("collate<char>::~collate: null object");
    _ZdlPv_nid_postfix(self);
}

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

int APS5_VABI _ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(const char* first1, const char* last1, const char* first2, const char* last2, const void* collvec) {
    (void)collvec;
    return sceLibcInternalCollateDoCompare_nid_no_patch(nullptr, first1, last1, first2, last2);
}

}

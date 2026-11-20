#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <stdexcept>

namespace {

const GuestLocale::WideNumpunctFacet* RequireWideNumpunct(const GuestLocale::WideNumpunctFacet* self) {
    if (self == nullptr || self->base.vtable == nullptr) throw std::invalid_argument("numpunct<wchar_t>: null facet");
    return self;
}

const GuestLocale::WideNumpunctVtable& WidePunctuation(const GuestLocale::WideNumpunctFacet* self) {
    return *reinterpret_cast<const GuestLocale::WideNumpunctVtable*>(self->base.vtable);
}

}

extern "C" {

char APS5_VABI _ZNKSt8numpunctIcE16do_decimal_pointEv(const GuestLocale::NumpunctFacet* self) {
    return NumpunctDoDecimalPoint_nid_no_patch(self);
}

char APS5_VABI _ZNKSt8numpunctIcE16do_thousands_sepEv(const GuestLocale::NumpunctFacet* self) {
    return NumpunctDoThousandsSep_nid_no_patch(self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE11do_groupingEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctDoGrouping_nid_no_patch(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE12do_falsenameEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctDoFalsename_nid_no_patch(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE11do_truenameEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctDoTruename_nid_no_patch(result, self);
}

char APS5_VABI _ZNKSt8numpunctIcE13decimal_pointEv(const GuestLocale::NumpunctFacet* self) {
    return NumpunctDecimalPoint_nid_no_patch(self);
}

char APS5_VABI _ZNKSt8numpunctIcE13thousands_sepEv(const GuestLocale::NumpunctFacet* self) {
    return NumpunctThousandsSep_nid_no_patch(self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE8groupingEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctGrouping_nid_no_patch(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE9falsenameEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctFalsename_nid_no_patch(result, self);
}

GuestLocale::String* APS5_VABI _ZNKSt8numpunctIcE8truenameEv(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return NumpunctTruename_nid_no_patch(result, self);
}

std::uint16_t APS5_VABI sceLibcInternalWideNumpunctDoDecimalPoint_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    return RequireWideNumpunct(self)->decimalPoint;
}
APS5_EXPORT("mzRlAVX65hQ", sceLibcInternalWideNumpunctDoDecimalPoint_nid_no_patch);

std::uint16_t APS5_VABI sceLibcInternalWideNumpunctDoThousandsSep_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    return RequireWideNumpunct(self)->thousandsSep;
}
APS5_EXPORT("Utj8Sh5L0jE", sceLibcInternalWideNumpunctDoThousandsSep_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctDoGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignCString_nid_no_patch(result, RequireWideNumpunct(self)->grouping);
}
APS5_EXPORT("2BmJdX269kI", sceLibcInternalWideNumpunctDoGrouping_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctDoFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignWideCString_nid_no_patch(result, RequireWideNumpunct(self)->falsename);
}
APS5_EXPORT("-amctzWbEtw", sceLibcInternalWideNumpunctDoFalsename_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctDoTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignWideCString_nid_no_patch(result, RequireWideNumpunct(self)->truename);
}
APS5_EXPORT("nvSsAW7tcX8", sceLibcInternalWideNumpunctDoTruename_nid_no_patch);

std::uint16_t APS5_VABI sceLibcInternalWideNumpunctDecimalPoint_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    const auto* facet = RequireWideNumpunct(self);
    return WidePunctuation(facet).decimalPoint(facet);
}
APS5_EXPORT("leSFwTZZuE4", sceLibcInternalWideNumpunctDecimalPoint_nid_no_patch);

std::uint16_t APS5_VABI sceLibcInternalWideNumpunctThousandsSep_nid_no_patch(const GuestLocale::WideNumpunctFacet* self) {
    const auto* facet = RequireWideNumpunct(self);
    return WidePunctuation(facet).thousandsSep(facet);
}
APS5_EXPORT("2Olt9gqOauQ", sceLibcInternalWideNumpunctThousandsSep_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    const auto* facet = RequireWideNumpunct(self);
    return WidePunctuation(facet).grouping(result, facet);
}
APS5_EXPORT("VsJCpXqMPJU", sceLibcInternalWideNumpunctGrouping_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    const auto* facet = RequireWideNumpunct(self);
    return WidePunctuation(facet).falsename(result, facet);
}
APS5_EXPORT("LDbKkgI-TZg", sceLibcInternalWideNumpunctFalsename_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalWideNumpunctTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    const auto* facet = RequireWideNumpunct(self);
    return WidePunctuation(facet).truename(result, facet);
}
APS5_EXPORT("3M20pLo9Gdk", sceLibcInternalWideNumpunctTruename_nid_no_patch);

}

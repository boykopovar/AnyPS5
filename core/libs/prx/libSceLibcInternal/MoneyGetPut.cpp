#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

const GuestLocale::CharMoneyGetVtable& CharMoneyGet(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::CharMoneyGetVtable*>(self->vtable);
}

const GuestLocale::WideMoneyGetVtable& WideMoneyGet(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::WideMoneyGetVtable*>(self->vtable);
}

const GuestLocale::CharMoneyPutVtable& CharMoneyPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::CharMoneyPutVtable*>(self->vtable);
}

const GuestLocale::WideMoneyPutVtable& WideMoneyPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::WideMoneyPutVtable*>(self->vtable);
}

}

extern "C" {

GuestLocale::InputIterator APS5_VABI sceLibcInternalMoneyGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return CharMoneyGet(self).getLongDouble(self, first, last, international, stream, state, value);
}
APS5_EXPORT("G84okRnyJJg", sceLibcInternalMoneyGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalMoneyGetString_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, GuestLocale::String* value) {
    return CharMoneyGet(self).getString(self, first, last, international, stream, state, value);
}
APS5_EXPORT("2fxdcyt5tGs", sceLibcInternalMoneyGetString_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideMoneyGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return WideMoneyGet(self).getLongDouble(self, first, last, international, stream, state, value);
}
APS5_EXPORT("cx-1THpef1A", sceLibcInternalWideMoneyGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideMoneyGetString_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, void* value) {
    return WideMoneyGet(self).getString(self, first, last, international, stream, state, value);
}
APS5_EXPORT("wWIsjOqfcSc", sceLibcInternalWideMoneyGetString_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalMoneyPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, char fill, long double units) {
    return CharMoneyPut(self).putLongDouble(self, out, international, stream, fill, units);
}
APS5_EXPORT("65cvm2NDLmU", sceLibcInternalMoneyPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalMoneyPutString_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, char fill, const GuestLocale::String* digits) {
    return CharMoneyPut(self).putString(self, out, international, stream, fill, digits);
}
APS5_EXPORT("DR029KeWsHw", sceLibcInternalMoneyPutString_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideMoneyPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, std::uint16_t fill, long double units) {
    return WideMoneyPut(self).putLongDouble(self, out, international, stream, fill, units);
}
APS5_EXPORT("fsF-tGtGsD4", sceLibcInternalWideMoneyPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideMoneyPutString_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, std::uint16_t fill, const void* digits) {
    return WideMoneyPut(self).putString(self, out, international, stream, fill, digits);
}
APS5_EXPORT("JruBeQgsAaU", sceLibcInternalWideMoneyPutString_nid_no_patch);

}

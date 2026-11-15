#include "prx/libSceLibcInternal/include/Api.hpp"
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

GuestLocale::InputIterator APS5_VABI _ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return CharMoneyGet(self).getLongDouble(self, first, last, international, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI sceLibcInternalMoneyGetString_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, GuestLocale::String* value) {
    return CharMoneyGet(self).getString(self, first, last, international, stream, state, value);
}
APS5_EXPORT("2fxdcyt5tGs", sceLibcInternalMoneyGetString_nid_no_patch);

GuestLocale::InputIterator APS5_VABI _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return WideMoneyGet(self).getLongDouble(self, first, last, international, stream, state, value);
}

GuestLocale::InputIterator APS5_VABI _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE_nid_postfix(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, void* value) {
    return WideMoneyGet(self).getString(self, first, last, international, stream, state, value);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basece_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, char fill, long double units) {
    return CharMoneyPut(self).putLongDouble(self, out, international, stream, fill, units);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basecRKSs_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, char fill, const GuestLocale::String* digits) {
    return CharMoneyPut(self).putString(self, out, international, stream, fill, digits);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewe_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, std::uint16_t fill, long double units) {
    return WideMoneyPut(self).putLongDouble(self, out, international, stream, fill, units);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewRKSbIwS2_SaIwEE_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, std::uint16_t fill, const void* digits) {
    return WideMoneyPut(self).putString(self, out, international, stream, fill, digits);
}

}

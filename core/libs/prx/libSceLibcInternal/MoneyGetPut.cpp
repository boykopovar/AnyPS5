#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

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

constexpr std::size_t MonetaryCategory = 3;
constexpr std::size_t WideElementSize = sizeof(std::uint16_t);

void APS5_VABI DestroyMoneyFacet(GuestLocale::Facet*) {}

void APS5_VABI DeleteMoneyFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("money facet delete: null object");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI RetainMoneyFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("money facet retain: null object");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseMoneyFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("money facet release: null object");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

GuestLocale::InputIterator APS5_VABI WideMoneyDoGetLongDouble(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator, bool, GuestLocale::IosBase*, std::int32_t*, long double*) {
    _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix();
    return first;
}

GuestLocale::InputIterator APS5_VABI WideMoneyDoGetString(const GuestLocale::Facet*, GuestLocale::InputIterator first, GuestLocale::InputIterator, bool, GuestLocale::IosBase*, std::int32_t*, void*) {
    _ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE_nid_postfix();
    return first;
}

GuestLocale::OutputIterator APS5_VABI WideMoneyDoPutLongDouble(const GuestLocale::Facet*, GuestLocale::OutputIterator out, bool, GuestLocale::IosBase*, std::uint16_t, long double) {
    _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewe_nid_postfix();
    return out;
}

GuestLocale::OutputIterator APS5_VABI WideMoneyDoPutString(const GuestLocale::Facet*, GuestLocale::OutputIterator out, bool, GuestLocale::IosBase*, std::uint16_t, const void*) {
    _ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewRKSbIwS2_SaIwEE_nid_postfix();
    return out;
}

const GuestLocale::WideMoneyGetVtable g_wideMoneyGetVtable{
    {DestroyMoneyFacet, DeleteMoneyFacet, RetainMoneyFacet, ReleaseMoneyFacet},
    WideMoneyDoGetLongDouble,
    WideMoneyDoGetString,
};

const GuestLocale::WideMoneyPutVtable g_wideMoneyPutVtable{
    {DestroyMoneyFacet, DeleteMoneyFacet, RetainMoneyFacet, ReleaseMoneyFacet},
    WideMoneyDoPutLongDouble,
    WideMoneyDoPutString,
};

void InitMoneyFacet(GuestLocale::Facet* self, const GuestLocale::FacetVtable* vtable, std::size_t refs) {
    self->vtable = vtable;
    self->references = static_cast<std::uint32_t>(refs);
    self->reserved = static_cast<std::uint32_t>(static_cast<std::uint64_t>(refs) >> 32);
}

void PutWideElement(GuestLocale::OutputIterator& out, std::uint16_t element) {
    if (out.failed) return;
    GuestLocale::Streambuf* buffer = out.buffer;
    if (buffer == nullptr) {
        out.failed = true;
        return;
    }
    char* next = buffer->putNext != nullptr ? *buffer->putNext : nullptr;
    char* end = buffer->putEnd != nullptr ? *buffer->putEnd : nullptr;
    if (next != nullptr && end != nullptr && end >= next && static_cast<std::size_t>(end - next) >= WideElementSize) {
        *buffer->putNext = next + WideElementSize;
        std::memcpy(next, &element, WideElementSize);
        return;
    }
    if (buffer->vtable == nullptr || buffer->vtable->overflow == nullptr || buffer->vtable->overflow(buffer, element) == -1) {
        out.failed = true;
    }
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

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    if (self == nullptr) throw std::invalid_argument("money_get<wchar_t>::money_get: null object");
    InitMoneyFacet(self, &g_wideMoneyGetVtable.facet, refs);
}

std::size_t APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const*) {
    if (facet != nullptr && *facet == nullptr) {
        auto* object = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(object, 0);
        *facet = object;
    }
    return MonetaryCategory;
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    if (self == nullptr) throw std::invalid_argument("money_put<wchar_t>::money_put: null object");
    if (info == nullptr) throw std::invalid_argument("money_put<wchar_t>::money_put: null _Locinfo");
    InitMoneyFacet(self, &g_wideMoneyPutVtable.facet, refs);
}

GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIwEEEm_nid_postfix(GuestLocale::OutputIterator out, const std::uint16_t* source, std::size_t count) {
    if (count != 0 && source == nullptr) throw std::invalid_argument("money_put<wchar_t>::_Put: null source");
    for (std::size_t index = 0; index < count && !out.failed; ++index) PutWideElement(out, source[index]);
    return out;
}

}

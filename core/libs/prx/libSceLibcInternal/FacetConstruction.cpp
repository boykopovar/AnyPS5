#include "prx/libSceLibcInternal/include/FacetConstruction.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include "prx/libSceLibcInternal/include/LocaleName.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libSceLibcInternal/include/TimeGetOrder.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using GuestLocale::Facet;
using GuestLocale::FacetVtable;
using WideChar = std::uint16_t;

constexpr std::size_t FacetObjectSize = 0x10;
constexpr int AllLocaleCategories = 0x3f;
constexpr char g_unnamedLocale[] = "*";
constexpr char g_charDays[] = ":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday";
constexpr char g_charMonths[] = ":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December";
constexpr char g_charAmPm[] = ":AM:am:PM:pm";
constexpr std::int32_t DateOrderMdy = 2;

struct GuestTimeGetCharFacet {
    Facet base;
    const char* days;
    const char* months;
    const char* amPm;
    std::int32_t dateOrder;
    std::uint32_t reserved;
    void* narrowState;
    void* wideState;
};

struct GuestTimePutNames {
    Facet base;
    GuestLocale::TimeInfo names;
};

static_assert(offsetof(GuestTimeGetCharFacet, days) == 0x10);
static_assert(offsetof(GuestTimeGetCharFacet, dateOrder) == 0x28);
static_assert(offsetof(GuestTimeGetCharFacet, narrowState) == 0x30);
static_assert(offsetof(GuestTimeGetCharFacet, wideState) == 0x38);
static_assert(sizeof(GuestTimeGetCharFacet) == 0x40);
static_assert(sizeof(GuestTimePutNames) == 0xb8);

void RequireObject(const void* object, const char* function) {
    if (object == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

void InstallHeader(Facet* self, const FacetVtable* vtable, std::size_t references) {
    self->vtable = vtable;
    self->references = static_cast<std::uint32_t>(references);
}

void APS5_VABI DestroyFacet(Facet* self) {
    RequireObject(self, "facet destructor");
}

void APS5_VABI DeleteFacetObject(Facet* self) {
    RequireObject(self, "facet deleting destructor");
    _ZdlPv_nid_postfix(self);
}

constexpr FacetVtable g_facetEntries{DestroyFacet, DeleteFacetObject, _ZNSt6locale5facet7_IncrefEv_nid_postfix, _ZNSt6locale5facet7_DecrefEv_nid_postfix};

template<typename Slot, typename Function> Slot AsSlot(Function* function) {
    return reinterpret_cast<Slot>(function);
}

const GuestLocale::NumGetVtable g_wideNumGetVtable{
    g_facetEntries,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERb_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
    _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERPv_nid_postfix};

GuestLocale::NumPutVtable MakeWideNumPutVtable() {
    GuestLocale::NumPutVtable vtable{};
    vtable.facet = g_facetEntries;
    vtable.putBool = AsSlot<decltype(vtable.putBool)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewb_nid_postfix);
    vtable.putLong = AsSlot<decltype(vtable.putLong)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewl_nid_postfix);
    vtable.putUnsignedLong = AsSlot<decltype(vtable.putUnsignedLong)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewm_nid_postfix);
    vtable.putLongLong = AsSlot<decltype(vtable.putLongLong)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewx_nid_postfix);
    vtable.putUnsignedLongLong = AsSlot<decltype(vtable.putUnsignedLongLong)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewy_nid_postfix);
    vtable.putDouble = AsSlot<decltype(vtable.putDouble)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewd_nid_postfix);
    vtable.putLongDouble = AsSlot<decltype(vtable.putLongDouble)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewe_nid_postfix);
    vtable.putPointer = AsSlot<decltype(vtable.putPointer)>(_ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_RSt8ios_basewPKv_nid_postfix);
    return vtable;
}

GuestLocale::CharMoneyGetVtable MakeCharMoneyGetVtable() {
    GuestLocale::CharMoneyGetVtable vtable{};
    vtable.facet = g_facetEntries;
    vtable.getLongDouble = AsSlot<decltype(vtable.getLongDouble)>(_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix);
    vtable.getString = AsSlot<decltype(vtable.getString)>(_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSs_nid_postfix);
    return vtable;
}

GuestLocale::WideMoneyGetVtable MakeWideMoneyGetVtable() {
    GuestLocale::WideMoneyGetVtable vtable{};
    vtable.facet = g_facetEntries;
    vtable.getLongDouble = AsSlot<decltype(vtable.getLongDouble)>(_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix);
    vtable.getString = AsSlot<decltype(vtable.getString)>(_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE_nid_postfix);
    return vtable;
}

GuestLocale::CharMoneyPutVtable MakeCharMoneyPutVtable() {
    GuestLocale::CharMoneyPutVtable vtable{};
    vtable.facet = g_facetEntries;
    vtable.putLongDouble = AsSlot<decltype(vtable.putLongDouble)>(_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basece_nid_postfix);
    vtable.putString = AsSlot<decltype(vtable.putString)>(_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE6do_putES3_bRSt8ios_basecRKSs_nid_postfix);
    return vtable;
}

GuestLocale::WideMoneyPutVtable MakeWideMoneyPutVtable() {
    GuestLocale::WideMoneyPutVtable vtable{};
    vtable.facet = g_facetEntries;
    vtable.putLongDouble = AsSlot<decltype(vtable.putLongDouble)>(_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewe_nid_postfix);
    vtable.putString = AsSlot<decltype(vtable.putString)>(_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE6do_putES3_bRSt8ios_basewRKSbIwS2_SaIwEE_nid_postfix);
    return vtable;
}

struct SyslockGuard {
    SyslockGuard() { _Locksyslock_nid_postfix(); }
    ~SyslockGuard() { _Unlocksyslock_nid_postfix(); }
    SyslockGuard(const SyslockGuard&) = delete;
    SyslockGuard& operator=(const SyslockGuard&) = delete;
};

void APS5_VABI DestroyLocimpFacet(Facet* self) {
    _ZNSt6locale7_LocimpD1Ev_nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(self));
}

void APS5_VABI DeleteLocimpFacet(Facet* self) {
    _ZNSt6locale7_LocimpD0Ev_nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(self));
}

constexpr FacetVtable g_locimpVtable{DestroyLocimpFacet, DeleteLocimpFacet, _ZNSt6locale5facet7_IncrefEv_nid_postfix, _ZNSt6locale5facet7_DecrefEv_nid_postfix};

void ConstructLocimp(GuestLocale::Implementation* self, bool transparent) {
    RequireObject(self, "locale::_Locimp::_Locimp");
    self->base.vtable = &g_locimpVtable;
    self->base.references = 1;
    self->facets = nullptr;
    self->facetCount = 0;
    self->category = 0;
    self->transparent = transparent;
    self->name = g_unnamedLocale;
}

std::int32_t APS5_VABI CharTimeGetDateOrder(const Facet* self) {
    return _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13do_date_orderEv_nid_postfix(self);
}

const GuestLocale::TimeGetVtable g_charTimeGetVtable{
    g_facetEntries,
    CharTimeGetDateOrder,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
    _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix};

const GuestLocale::NumPutVtable g_wideNumPutVtable = MakeWideNumPutVtable();
const GuestLocale::CharMoneyGetVtable g_charMoneyGetVtable = MakeCharMoneyGetVtable();
const GuestLocale::WideMoneyGetVtable g_wideMoneyGetVtable = MakeWideMoneyGetVtable();
const GuestLocale::CharMoneyPutVtable g_charMoneyPutVtable = MakeCharMoneyPutVtable();
const GuestLocale::WideMoneyPutVtable g_wideMoneyPutVtable = MakeWideMoneyPutVtable();

void InitNumGetChar(Facet* self, std::size_t references) {
    InstallHeader(self, LibcInternalCharNumGetVtable_nid_no_patch(), references);
}

void InitNumGetWide(Facet* self, std::size_t references) {
    InstallHeader(self, &g_wideNumGetVtable.facet, references);
}

void InitNumPutChar(Facet* self, std::size_t references) {
    InstallHeader(self, reinterpret_cast<const FacetVtable*>(&_ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[2]), references);
}

void InitNumPutWide(Facet* self, std::size_t references) {
    InstallHeader(self, &g_wideNumPutVtable.facet, references);
}

void InitMoneyGetChar(Facet* self, std::size_t references) {
    InstallHeader(self, &g_charMoneyGetVtable.facet, references);
}

void InitMoneyGetWide(Facet* self, std::size_t references) {
    InstallHeader(self, &g_wideMoneyGetVtable.facet, references);
}

void InitMoneyPutChar(Facet* self, std::size_t references) {
    InstallHeader(self, &g_charMoneyPutVtable.facet, references);
}

void InitMoneyPutWide(Facet* self, std::size_t references) {
    InstallHeader(self, &g_wideMoneyPutVtable.facet, references);
}

void InitTimeGetChar(Facet* self, std::size_t references) {
    auto* facet = reinterpret_cast<GuestTimeGetCharFacet*>(self);
    InstallHeader(&facet->base, &g_charTimeGetVtable.facet, references);
    facet->days = g_charDays;
    facet->months = g_charMonths;
    facet->amPm = g_charAmPm;
    facet->dateOrder = DateOrderMdy;
    facet->narrowState = _Getpmbstate_nid_postfix();
    facet->wideState = _Getpwcstate_nid_postfix();
}

void InstallCollationStates(GuestLocale::CollateFacet* self) {
    self->collation = _Getpcostate_nid_postfix();
    self->wideCollation = _Getpwcostate_nid_postfix();
}

void InitCollateWide(GuestLocale::CollateFacet* self, std::size_t references) {
    InstallHeader(&self->base, &GuestWideCollateVtable_nid_no_patch()->facet, references);
    InstallCollationStates(self);
}

void InstallTimeNames(Facet* self) {
    reinterpret_cast<GuestTimePutNames*>(self)->names = *_Getptimes_nid_postfix();
}

template<typename Object, void (*Init)(Object*, std::size_t)> void ConstructWithReferences(Object* self, std::size_t references, const char* function) {
    RequireObject(self, function);
    Init(self, references);
}

template<typename Object, void (*Init)(Object*, std::size_t)> void ConstructWithName(Object* self, const char* localeName, std::size_t references, const char* function) {
    RequireObject(self, function);
    Init(self, references);
    _ZSt15_sceLibcLocinfoPKc_nid_postfix(localeName);
}

template<typename Object, void (*Init)(Object*, std::size_t)> void ConstructWithLocinfo(Object* self, const GuestLocale::LocinfoStorage* info, std::size_t references, const char* function) {
    RequireObject(self, function);
    RequireObject(info, function);
    Init(self, references);
}

template<typename Object, void (*Init)(Object*, std::size_t)> std::size_t Getcat(Facet** facet, const GuestLocale::Implementation* const* locale, std::size_t category) {
    if (facet == nullptr || *facet != nullptr) return category;
    struct ObjectGuard {
        void* object;
        ~ObjectGuard() {
            if (object != nullptr) _ZdlPv_nid_postfix(object);
        }
    } guard{_Znwm_nid_postfix(FacetObjectSize)};
    const char* named = locale != nullptr && *locale != nullptr ? (*locale)->name : "";
    _ZSt15_sceLibcLocinfoPKc_nid_postfix(named);
    Init(static_cast<Object*>(guard.object), 0);
    *facet = static_cast<Facet*>(guard.object);
    guard.object = nullptr;
    return category;
}

template<typename Element> GuestLocale::OutputIterator PutElement(GuestLocale::OutputIterator out, Element element) {
    GuestLocale::Streambuf* buffer = out.buffer;
    if (buffer == nullptr) {
        out.failed = true;
        return out;
    }
    char* next = *buffer->putNext;
    if (next != nullptr) {
        const auto nextAddress = reinterpret_cast<std::uintptr_t>(next);
        const auto endAddress = reinterpret_cast<std::uintptr_t>(*buffer->putEnd);
        const std::uint32_t available = sizeof(Element) == 1 ? static_cast<std::uint32_t>(endAddress) - static_cast<std::uint32_t>(nextAddress) : static_cast<std::uint32_t>((endAddress - nextAddress) >> 1);
        if (static_cast<std::int32_t>(available) > 0) {
            *buffer->putNext = next + sizeof(Element);
            std::memcpy(next, &element, sizeof(Element));
            return out;
        }
    }
    if (buffer->vtable->overflow(buffer, static_cast<std::make_unsigned_t<Element>>(element)) == -1) out.failed = true;
    return out;
}

template<typename Element> GuestLocale::OutputIterator PutElements(GuestLocale::OutputIterator out, const Element* elements, std::size_t count) {
    for (std::size_t index = 0; index != count; ++index) out = PutElement<Element>(out, elements[index]);
    return out;
}

template<typename Element> GuestLocale::OutputIterator RepeatElement(GuestLocale::OutputIterator out, Element element, std::size_t count) {
    for (; count != 0; --count) out = PutElement<Element>(out, element);
    return out;
}

}

extern "C" {

void APS5_VABI _ZNSt7collateIwE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info) {
    RequireObject(self, "collate<wchar_t>::_Init");
    RequireObject(info, "collate<wchar_t>::_Init _Locinfo");
    InstallCollationStates(self);
}

void APS5_VABI _ZNSt7collateIwEC1EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructWithName<GuestLocale::CollateFacet, InitCollateWide>(self, localeName, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt7collateIwEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::CollateFacet, InitCollateWide>(self, info, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt7collateIwEC2EPKcm_nid_postfix(GuestLocale::CollateFacet* self, const char* localeName, std::size_t references) {
    ConstructWithName<GuestLocale::CollateFacet, InitCollateWide>(self, localeName, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt7collateIwEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::CollateFacet, InitCollateWide>(self, info, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt7collateIwEC2Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::CollateFacet, InitCollateWide>(self, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumGetChar>(self, references, "num_get<char>::num_get");
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumGetChar>(self, info, references, "num_get<char>::num_get");
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumGetChar>(self, references, "num_get<char>::num_get");
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumGetWide>(self, info, references, "num_get<wchar_t>::num_get");
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumGetWide>(self, references, "num_get<wchar_t>::num_get");
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumGetWide>(self, info, references, "num_get<wchar_t>::num_get");
}

void APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumGetWide>(self, references, "num_get<wchar_t>::num_get");
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumPutChar>(self, info, references, "num_put<char>::num_put");
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumPutChar>(self, references, "num_put<char>::num_put");
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumPutChar>(self, info, references, "num_put<char>::num_put");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitTimeGetChar>(self, info, references, "time_get<char>::time_get");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructWithName<GuestLocale::Facet, InitTimeGetChar>(self, localeName, references, "time_get<char>::time_get");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitTimeGetChar>(self, info, references, "time_get<char>::time_get");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitTimeGetChar>(self, references, "time_get<char>::time_get");
}

std::size_t APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return Getcat<GuestLocale::Facet, InitMoneyGetChar>(facet, locale, 3);
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyGetChar>(self, info, references, "money_get<char>::money_get");
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyGetChar>(self, references, "money_get<char>::money_get");
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyGetChar>(self, info, references, "money_get<char>::money_get");
}

void APS5_VABI _ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyGetChar>(self, references, "money_get<char>::money_get");
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyGetWide>(self, info, references, "money_get<wchar_t>::money_get");
}

std::size_t APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return Getcat<GuestLocale::Facet, InitMoneyPutChar>(facet, locale, 3);
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyPutChar>(self, info, references, "money_put<char>::money_put");
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyPutChar>(self, references, "money_put<char>::money_put");
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyPutChar>(self, info, references, "money_put<char>::money_put");
}

void APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyPutChar>(self, references, "money_put<char>::money_put");
}

std::size_t APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return Getcat<GuestLocale::Facet, InitMoneyPutWide>(facet, locale, 3);
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyPutWide>(self, references, "money_put<wchar_t>::money_put");
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyPutWide>(self, references, "money_put<wchar_t>::money_put");
}

void APS5_VABI _ZNSt7collateIcE5_InitERKSt8_Locinfo_nid_postfix(GuestLocale::CollateFacet* self, const GuestLocale::LocinfoStorage* info) {
    RequireObject(self, "collate<char>::_Init");
    RequireObject(info, "collate<char>::_Init _Locinfo");
    InstallCollationStates(self);
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyGetWide>(self, info, references, "money_get<wchar_t>::money_get");
}

void APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitMoneyPutWide>(self, info, references, "money_put<wchar_t>::money_put");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1EPKcm_nid_postfix(GuestLocale::Facet* self, const char* localeName, std::size_t references) {
    ConstructWithName<GuestLocale::Facet, InitTimeGetChar>(self, localeName, references, "time_get<char>::time_get");
}

void APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    ConstructWithLocinfo<GuestLocale::Facet, InitNumGetChar>(self, info, references, "num_get<char>::num_get");
}

void APS5_VABI _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitNumPutChar>(self, references, "num_put<char>::num_put");
}

void APS5_VABI _ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitMoneyGetWide>(self, references, "money_get<wchar_t>::money_get");
}

void APS5_VABI _ZNSt7collateIwEC1Em_nid_postfix(GuestLocale::CollateFacet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::CollateFacet, InitCollateWide>(self, references, "collate<wchar_t>::collate");
}

void APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t references) {
    ConstructWithReferences<GuestLocale::Facet, InitTimeGetChar>(self, references, "time_get<char>::time_get");
}

std::size_t APS5_VABI _ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return Getcat<GuestLocale::Facet, InitNumGetWide>(facet, locale, 4);
}

void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    RequireObject(self, "time_put<wchar_t>::time_put");
    RequireObject(info, "time_put<wchar_t>::time_put _Locinfo");
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(self, references);
    InstallTimeNames(self);
}

void APS5_VABI _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t references) {
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(self, info, references);
}

void APS5_VABI _ZNSt5ctypeIwED1Ev_nid_postfix(GuestLocale::CtypeFacet* self) {
    RequireObject(self, "ctype<wchar_t>::~ctype");
    if (self->deleteTable != 0) free_nid_postfix(const_cast<std::int16_t*>(self->classification));
}

void APS5_VABI _ZNSt5ctypeIwED0Ev_nid_postfix(GuestLocale::CtypeFacet* self) {
    _ZNSt5ctypeIwED1Ev_nid_postfix(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN6Dinkum7codecvt10_Cvt_checkEmm_nid_postfix(std::size_t, std::size_t) {}

void APS5_VABI _ZNSt8_LocinfoC2EiPKc_nid_postfix(GuestLocinfoLayout* self, int category, const char* localeName) {
    RequireObject(self, "_Locinfo::_Locinfo");
    (void)category;
    self->times = *_Getptimes_nid_postfix();
    const char* current = setlocale_nid_postfix(AllLocaleCategories, nullptr);
    self->name = current != nullptr ? current : "";
    _ZSt15_sceLibcLocinfoPKc_nid_postfix(localeName);
    self->classicName = setlocale_nid_postfix(AllLocaleCategories, nullptr);
}

GuestLocinfoLayout* APS5_VABI _ZNSt8_Locinfo8_AddcatsEiPKc_nid_postfix(GuestLocinfoLayout* self, int category, const char* localeName) {
    RequireObject(self, "_Locinfo::_Addcats");
    (void)category;
    _ZSt15_sceLibcLocinfoPKc_nid_postfix(localeName);
    self->classicName = setlocale_nid_postfix(AllLocaleCategories, nullptr);
    return self;
}

void APS5_VABI _ZNSt8_LocinfoC1ERKSs_nid_postfix(GuestLocinfoLayout* self, const GuestLocale::String* name) {
    RequireObject(self, "_Locinfo::_Locinfo");
    RequireObject(name, "_Locinfo::_Locinfo string");
    self->times = *_Getptimes_nid_postfix();
    const char* current = setlocale_nid_postfix(AllLocaleCategories, nullptr);
    self->name = current;
    _ZSt15_sceLibcLocinfoPKc_nid_postfix(name->capacity >= 0x10 ? name->pointer : name->buffer);
    self->classicName = current;
}

void APS5_VABI _ZNSt8_LocinfoC2ERKSs_nid_postfix(GuestLocinfoLayout* self, const GuestLocale::String* name) {
    _ZNSt8_LocinfoC1ERKSs_nid_postfix(self, name);
}

void APS5_VABI _ZNSt6locale7_LocimpD1Ev_nid_postfix(GuestLocale::Implementation* self) {
    RequireObject(self, "locale::_Locimp::~_Locimp");
    self->base.vtable = &g_locimpVtable;
    SyslockGuard syslock;
    for (std::size_t index = self->facetCount; index-- != 0;) {
        if (Facet* facet = self->facets[index]) {
            if (Facet* released = facet->vtable->release(facet)) released->vtable->deleteObject(released);
        }
    }
    if (self->facets != _ZNSt6locale7classicEv_nid_postfix()->implementation->facets) free_nid_postfix(self->facets);
}

void APS5_VABI _ZNSt6locale7_LocimpD2Ev_nid_postfix(GuestLocale::Implementation* self) {
    _ZNSt6locale7_LocimpD1Ev_nid_postfix(self);
}

void APS5_VABI _ZNSt6locale7_LocimpD0Ev_nid_postfix(GuestLocale::Implementation* self) {
    _ZNSt6locale7_LocimpD2Ev_nid_postfix(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZNSt6locale7_LocimpC1Eb_nid_postfix(GuestLocale::Implementation* self, bool transparent) {
    ConstructLocimp(self, transparent);
}

void APS5_VABI _ZNSt6localeD1Ev_nid_postfix(GuestLocaleFacade* self) {
    RequireObject(self, "locale::~locale");
    GuestLocale::Implementation* implementation = self->implementation;
    if (implementation == nullptr) return;
    if (Facet* released = implementation->base.vtable->release(&implementation->base)) released->vtable->deleteObject(released);
}

GuestLocaleFacade* APS5_VABI _ZNSt6locale5emptyEv_nid_postfix(GuestLocaleFacade* result) {
    RequireObject(result, "locale::empty");
    _ZNSt6locale5_InitEv_nid_postfix();
    auto* implementation = static_cast<GuestLocale::Implementation*>(_Znwm_nid_postfix(sizeof(GuestLocale::Implementation)));
    ConstructLocimp(implementation, true);
    result->implementation = implementation;
    return result;
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_PKcm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, const char* elements, std::size_t count) {
    RequireObject(self, "num_put<char>::_Put");
    return PutElements<char>(out, elements, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, char element, std::size_t count) {
    RequireObject(self, "num_put<char>::_Rep");
    return RepeatElement<char>(out, element, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIcEEEm_nid_postfix(GuestLocale::OutputIterator out, GuestStringConstIterator<char> first, std::size_t count) {
    return PutElements<char>(out, first.pointer, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm_nid_postfix(GuestLocale::OutputIterator out, char element, std::size_t count) {
    return RepeatElement<char>(out, element, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(GuestLocale::OutputIterator out, WideChar element, std::size_t count) {
    return RepeatElement<WideChar>(out, element, count);
}

}

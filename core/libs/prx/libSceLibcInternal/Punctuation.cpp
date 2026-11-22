#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/FacetReferences.hpp"
#include "prx/libSceLibcInternal/include/Punctuation.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

using WideChar = std::uint16_t;

constexpr char g_empty[] = "";
constexpr WideChar g_wideFalseName[] = {'f', 'a', 'l', 's', 'e', 0};
constexpr WideChar g_wideTrueName[] = {'t', 'r', 'u', 'e', 0};

void RequireObject(const void* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

WideChar Widen(char byte) {
    return static_cast<unsigned char>(byte);
}

struct Separators {
    char decimalPoint;
    char thousandsSep;
};

Separators SeparatorsOf(const GuestLocale::LocinfoStorage* locinfo, bool isDefault) {
    RequireObject(locinfo, "numpunct::_Init _Locinfo");
    return isDefault ? Separators{'.', ','} : Separators{'.', '\0'};
}

void SetReferences(GuestLocale::Facet* self, std::size_t references) {
    self->references = static_cast<std::uint32_t>(references);
}

struct ScopedLocinfo {
    GuestLocale::LocinfoStorage storage{};

    explicit ScopedLocinfo(const char* localeName) {
        _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, localeName);
    }

    ~ScopedLocinfo() {
        _ZNSt8_LocinfoD1Ev_nid_postfix(&storage);
    }

    ScopedLocinfo(const ScopedLocinfo&) = delete;
    ScopedLocinfo& operator=(const ScopedLocinfo&) = delete;
};

void InitWideNumpunct(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, bool isDefault) {
    const Separators separators = SeparatorsOf(locinfo, isDefault);
    self->grouping = g_empty;
    self->falsename = g_wideFalseName;
    self->truename = g_wideTrueName;
    self->decimalPoint = Widen(separators.decimalPoint);
    self->thousandsSep = Widen(separators.thousandsSep);
}

void ConstructWideNumpunct(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    self->base.vtable = LibcWideNumpunctVtable_nid_no_patch();
    SetReferences(&self->base, references);
}

void ConstructClassicWideNumpunct(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    RequireObject(self, "numpunct<wchar_t>::numpunct");
    ConstructWideNumpunct(self, references);
    ScopedLocinfo locinfo("C");
    InitWideNumpunct(self, &locinfo.storage, false);
    if (self->thousandsSep == 0) self->thousandsSep = Widen(',');
}

void ConstructNamedWideNumpunct(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool isDefault) {
    RequireObject(self, "numpunct<wchar_t>::numpunct");
    ConstructWideNumpunct(self, references);
    ScopedLocinfo locinfo(localeName);
    InitWideNumpunct(self, &locinfo.storage, isDefault);
}

void ConstructLocinfoWideNumpunct(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool isDefault) {
    RequireObject(self, "numpunct<wchar_t>::numpunct");
    RequireObject(locinfo, "numpunct<wchar_t>::numpunct _Locinfo");
    ConstructWideNumpunct(self, references);
    InitWideNumpunct(self, locinfo, isDefault);
}

void DeleteFacet(void* self, const char* function) {
    RequireObject(self, function);
    _ZdlPv_nid_postfix(self);
}

}

extern "C" {

void APS5_VABI _ZNSt8numpunctIwE5_InitERKSt8_Locinfob_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, bool isDefault) {
    RequireObject(self, "numpunct<wchar_t>::_Init");
    InitWideNumpunct(self, locinfo, isDefault);
}

void APS5_VABI _ZNSt8numpunctIwEC1Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    ConstructClassicWideNumpunct(self, references);
}

void APS5_VABI _ZNSt8numpunctIwEC2Em_nid_postfix(GuestLocale::WideNumpunctFacet* self, std::size_t references) {
    ConstructClassicWideNumpunct(self, references);
}

void APS5_VABI _ZNSt8numpunctIwEC1EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool isDefault) {
    ConstructNamedWideNumpunct(self, localeName, references, isDefault);
}

void APS5_VABI _ZNSt8numpunctIwEC2EPKcmb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const char* localeName, std::size_t references, bool isDefault) {
    ConstructNamedWideNumpunct(self, localeName, references, isDefault);
}

void APS5_VABI _ZNSt8numpunctIwEC1ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool isDefault) {
    ConstructLocinfoWideNumpunct(self, locinfo, references, isDefault);
}

void APS5_VABI _ZNSt8numpunctIwEC2ERKSt8_Locinfomb_nid_postfix(GuestLocale::WideNumpunctFacet* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references, bool isDefault) {
    ConstructLocinfoWideNumpunct(self, locinfo, references, isDefault);
}

void APS5_VABI _ZNSt8numpunctIwED1Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self) {
    RequireObject(self, "numpunct<wchar_t>::~numpunct");
}

void APS5_VABI _ZNSt8numpunctIwED2Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self) {
    RequireObject(self, "numpunct<wchar_t>::~numpunct");
}

void APS5_VABI _ZNSt8numpunctIwED0Ev_nid_postfix(GuestLocale::WideNumpunctFacet* self) {
    DeleteFacet(self, "numpunct<wchar_t>::~numpunct");
}

}

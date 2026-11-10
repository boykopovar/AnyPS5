#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal moneypunct: %s\n", message);
    std::abort();
}

std::string Text(const GuestLocale::String& string) {
    return {string.capacity > 15 ? string.pointer : string.buffer, string.size};
}

std::string CallChar(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::MoneyPunctFacet*), const GuestLocale::MoneyPunctFacet* facet) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the string result is the caller's buffer");
    Require(result.capacity == 15, "a short string stays inline");
    return Text(result);
}

std::string CallWide(GuestLocale::String* (APS5_VABI* method)(GuestLocale::String*, const GuestLocale::WideMoneyPunctFacet*), const GuestLocale::WideMoneyPunctFacet* facet) {
    GuestLocale::String result{};
    Require(method(&result, facet) == &result, "the wide string result is the caller's buffer");
    return Text(result);
}

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

GuestLocale::String* APS5_VABI DerivedCurrSymbol(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet*) {
    *result = {};
    result->size = 3;
    result->capacity = 15;
    std::memcpy(result->buffer, "EUR", 3);
    return result;
}

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
char APS5_VABI DerivedDecimalPoint(const GuestLocale::MoneyPunctFacet*) { return ';'; }
char APS5_VABI DerivedThousandsSep(const GuestLocale::MoneyPunctFacet*) { return '_'; }
GuestLocale::String* APS5_VABI DerivedGrouping(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet*) {
    *result = {};
    result->size = 1;
    result->capacity = 15;
    result->buffer[0] = 3;
    return result;
}
GuestLocale::String* DerivedEmpty(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet*) {
    *result = {};
    result->size = 0;
    result->capacity = 15;
    return result;
}
char DerivedFracDigits(const GuestLocale::MoneyPunctFacet*) { return 2; }
std::uint32_t DerivedPosFormat(const GuestLocale::MoneyPunctFacet*) { return 0x04010303; }
std::uint32_t DerivedNegFormat(const GuestLocale::MoneyPunctFacet*) { return 0x04010303; }

const GuestLocale::MoneyPunctVtable g_derivedVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, DerivedDecimalPoint, DerivedThousandsSep, DerivedGrouping, DerivedCurrSymbol, DerivedEmpty, DerivedEmpty, DerivedFracDigits, DerivedPosFormat, DerivedNegFormat};

constexpr std::uint32_t Pattern(char first, char second, char third, char fourth) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(first)) | static_cast<std::uint32_t>(static_cast<unsigned char>(second)) << 8 | static_cast<std::uint32_t>(static_cast<unsigned char>(third)) << 16 | static_cast<std::uint32_t>(static_cast<unsigned char>(fourth)) << 24;
}

bool ThrowsOnNonClassicName(GuestLocale::MoneyPunctFacet* facet) {
    try {
        _ZNSt10moneypunctIcLb0EEC2EPKcm_nid_postfix(facet, "fr_FR.UTF-8", 0);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&HeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&HeapFree);
    heapApi[2] = reinterpret_cast<void*>(&HeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&HeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&HeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&HeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    GuestLocale::MoneyPunctFacet classic{};
    _ZNSt10moneypunctIcLb0EEC2EPKcm_nid_postfix(&classic, "C", 0);
    Require(classic.base.vtable != nullptr, "the constructor installs a vtable");
    Require(classic.decimalPoint == '.', "decimal_point of the C locale is '.'");
    Require(classic.thousandsSep == ',', "thousands_sep of the C locale is ','");
    Require(std::strcmp(classic.grouping, "") == 0, "grouping of the C locale is empty");
    Require(std::strcmp(classic.currSymbol, "") == 0, "curr_symbol of the C locale is empty");
    Require(std::strcmp(classic.positiveSign, "") == 0, "positive_sign of the C locale is empty");
    Require(std::strcmp(classic.negativeSign, "") == 0, "negative_sign of the C locale is empty");
    Require(classic.fracDigits == CHAR_MAX, "frac_digits of the C locale is CHAR_MAX");
    Require(classic.posFormat == Pattern(2, 3, 0, 4), "pos_format is symbol, sign, none, value");
    Require(classic.negFormat == Pattern(3, 2, 0, 4), "neg_format is sign, symbol, none, value");

    Require(sceLibcInternalMoneyPunctDoDecimalPointChar_nid_no_patch(&classic) == '.', "do_decimal_point of the C locale is '.'");
    Require(sceLibcInternalMoneyPunctDoThousandsSepChar_nid_no_patch(&classic) == ',', "do_thousands_sep of the C locale is ','");
    Require(sceLibcInternalMoneyPunctDoFracDigitsChar_nid_no_patch(&classic) == CHAR_MAX, "do_frac_digits of the C locale is CHAR_MAX");
    Require(sceLibcInternalMoneyPunctDoPosFormatChar_nid_no_patch(&classic) == Pattern(2, 3, 0, 4), "do_pos_format reads the facet's pattern");
    Require(sceLibcInternalMoneyPunctDoNegFormatChar_nid_no_patch(&classic) == Pattern(3, 2, 0, 4), "do_neg_format reads the facet's pattern");
    Require(CallChar(sceLibcInternalMoneyPunctDoGroupingChar_nid_no_patch, &classic).empty(), "do_grouping of the C locale is empty");
    Require(CallChar(sceLibcInternalMoneyPunctDoCurrSymbolChar_nid_no_patch, &classic).empty(), "do_curr_symbol of the C locale is empty");
    Require(CallChar(sceLibcInternalMoneyPunctDoPositiveSignChar_nid_no_patch, &classic).empty(), "do_positive_sign of the C locale is empty");
    Require(CallChar(sceLibcInternalMoneyPunctDoNegativeSignChar_nid_no_patch, &classic).empty(), "do_negative_sign of the C locale is empty");

    GuestLocale::MoneyPunctFacet viaReferences{};
    _ZNSt10moneypunctIcLb0EEC1Em_nid_postfix(&viaReferences, 5);
    Require(viaReferences.base.references == 5, "the reference count comes from the constructor argument");
    _ZNSt10moneypunctIcLb0EEC2Em_nid_postfix(&viaReferences, 6);
    Require(viaReferences.base.references == 6, "the base-object constructor takes the reference count too");

    GuestLocale::MoneyPunctFacet international{};
    _ZNSt10moneypunctIcLb1EEC2Em_nid_postfix(&international, 0);
    Require(std::strcmp(international.currSymbol, "") == 0, "the international facet of the C locale has no currency symbol");
    Require(international.fracDigits == CHAR_MAX, "the international facet of the C locale keeps CHAR_MAX fractions");
    GuestLocale::LocinfoStorage info{};
    _ZNSt10moneypunctIcLb0EEC1ERKSt8_Locinfomb_nid_postfix(&international, &info, 1, true);
    _ZNSt10moneypunctIcLb0EEC2ERKSt8_Locinfomb_nid_postfix(&international, &info, 2, false);
    Require(international.base.references == 2, "the _Locinfo constructor takes the reference count");
    _ZNSt10moneypunctIcLb1EEC2ERKSt8_Locinfomb_nid_postfix(&international, &info, 0, true);

    Require(ThrowsOnNonClassicName(&classic), "constructing from a name other than \"C\" throws");

    _ZNSt10moneypunctIcLb1EED1Ev_nid_postfix(&classic);
    _ZNSt10moneypunctIcLb1EED2Ev_nid_postfix(&classic);

    GuestLocale::MoneyPunctFacet derived{{&g_derivedVtable.facet, 1, 0}, "\3", ';', '_', 2, 0x04010303, 0x04010303, "EUR", "+", "-"};
    Require(sceLibcInternalMoneyPunctDoDecimalPointChar_nid_no_patch(&derived) == ';', "do_decimal_point reads the facet's field");
    Require(sceLibcInternalMoneyPunctDoThousandsSepChar_nid_no_patch(&derived) == '_', "do_thousands_sep reads the facet's field");
    Require(CallChar(sceLibcInternalMoneyPunctDoGroupingChar_nid_no_patch, &derived) == "\3", "do_grouping copies the facet's grouping");
    Require(CallChar(sceLibcInternalMoneyPunctDoCurrSymbolChar_nid_no_patch, &derived) == "EUR", "do_curr_symbol copies the facet's currency symbol");
    Require(CallChar(sceLibcInternalMoneyPunctDoPositiveSignChar_nid_no_patch, &derived) == "+", "do_positive_sign copies the facet's positive sign");
    Require(CallChar(sceLibcInternalMoneyPunctDoNegativeSignChar_nid_no_patch, &derived) == "-", "do_negative_sign copies the facet's negative sign");
    Require(sceLibcInternalMoneyPunctDoFracDigitsChar_nid_no_patch(&derived) == 2, "do_frac_digits reads the facet's field");
    Require(sceLibcInternalMoneyPunctDoPosFormatChar_nid_no_patch(&derived) == 0x04010303, "do_pos_format reads the facet's field");
    Require(reinterpret_cast<const GuestLocale::MoneyPunctVtable*>(derived.base.vtable)->currSymbol == DerivedCurrSymbol, "the derived vtable keeps its own do_curr_symbol");
    GuestLocale::String throughVtable{};
    reinterpret_cast<const GuestLocale::MoneyPunctVtable*>(derived.base.vtable)->currSymbol(&throughVtable, &derived);
    Require(Text(throughVtable) == "EUR", "the vtable slot reaches the overriding member");

    GuestLocale::WideMoneyPunctFacet wide{};
    _ZNSt7_MpunctIwEC2Emb_nid_postfix(&wide, 3, true);
    Require(wide.base.references == 3, "the wide constructor takes the reference count");
    Require(wide.decimalPoint == '.', "the wide facet keeps '.' as its decimal point");
    Require(wide.thousandsSep == ',', "the wide facet keeps ',' as its thousands separator");
    Require(wide.fracDigits == CHAR_MAX, "the wide facet keeps CHAR_MAX fractions");
    Require(wide.posFormat == Pattern(2, 3, 0, 4), "the wide facet pos_format is symbol, sign, none, value");
    Require(wide.negFormat == Pattern(3, 2, 0, 4), "the wide facet neg_format is sign, symbol, none, value");
    Require(sceLibcInternalMoneyPunctDoDecimalPointWide_nid_no_patch(&wide) == '.', "wide do_decimal_point reads the facet's field");
    Require(sceLibcInternalMoneyPunctDoThousandsSepWide_nid_no_patch(&wide) == ',', "wide do_thousands_sep reads the facet's field");
    Require(sceLibcInternalMoneyPunctDoFracDigitsWide_nid_no_patch(&wide) == CHAR_MAX, "wide do_frac_digits reads the facet's field");
    Require(CallWide(sceLibcInternalMoneyPunctDoCurrSymbolWide_nid_no_patch, &wide).empty(), "wide do_curr_symbol of the C locale is empty");
    Require(CallWide(sceLibcInternalMoneyPunctDoPositiveSignWide_nid_no_patch, &wide).empty(), "wide do_positive_sign of the C locale is empty");
    Require(CallWide(sceLibcInternalMoneyPunctDoNegativeSignWide_nid_no_patch, &wide).empty(), "wide do_negative_sign of the C locale is empty");
    Require(CallWide(sceLibcInternalMoneyPunctDoGroupingWide_nid_no_patch, &wide).empty(), "wide do_grouping of the C locale is empty");

    GuestLocale::WideMoneyPunctFacet wideDefault{};
    _ZNSt10moneypunctIwLb0EEC1EPKcm_nid_postfix(&wideDefault, "C", 0);
    _ZNSt10moneypunctIwLb0EEC1Em_nid_postfix(&wideDefault, 1);
    _ZNSt10moneypunctIwLb1EEC2ERKSt8_Locinfomb_nid_postfix(&wideDefault, &info, 2, true);
    Require(wideDefault.base.references == 2 && wideDefault.base.vtable != nullptr, "the wide moneypunct constructors install the wide vtable");

    auto* heapFacet = static_cast<GuestLocale::MoneyPunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::MoneyPunctFacet)));
    _ZNSt10moneypunctIcLb0EEC2Em_nid_postfix(heapFacet, 1);
    _ZNSt10moneypunctIcLb0EED0Ev_nid_postfix(heapFacet);
    auto* heapWide = static_cast<GuestLocale::WideMoneyPunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::WideMoneyPunctFacet)));
    _ZNSt7_MpunctIwEC2Emb_nid_postfix(heapWide, 1, false);
    _ZNSt7_MpunctIwED0Ev_nid_postfix(heapWide);
    auto* heapChar = static_cast<GuestLocale::MoneyPunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::MoneyPunctFacet)));
    _ZNSt10moneypunctIcLb0EEC2Em_nid_postfix(heapChar, 1);
    _ZNSt7_MpunctIcED0Ev_nid_postfix(heapChar);
    auto* heapIntl = static_cast<GuestLocale::MoneyPunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::MoneyPunctFacet)));
    _ZNSt10moneypunctIcLb1EEC2Em_nid_postfix(heapIntl, 1);
    _ZNSt10moneypunctIcLb1EED0Ev_nid_postfix(heapIntl);
    auto* heapWideIntl = static_cast<GuestLocale::WideMoneyPunctFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::WideMoneyPunctFacet)));
    _ZNSt10moneypunctIwLb1EEC2ERKSt8_Locinfomb_nid_postfix(heapWideIntl, &info, 1, true);
    _ZNSt10moneypunctIwLb1EED0Ev_nid_postfix(heapWideIntl);

    return 0;
}

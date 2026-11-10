#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

char APS5_VABI sceLibcInternalMoneyPunctDoDecimalPointChar_nid_no_patch(const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE16do_decimal_pointEv_nid_postfix(self);
}
APS5_EXPORT("tZj4yquwuhI", sceLibcInternalMoneyPunctDoDecimalPointChar_nid_no_patch);

char APS5_VABI sceLibcInternalMoneyPunctDoThousandsSepChar_nid_no_patch(const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE16do_thousands_sepEv_nid_postfix(self);
}
APS5_EXPORT("6aFwTNpqTP8", sceLibcInternalMoneyPunctDoThousandsSepChar_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoGroupingChar_nid_no_patch(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE11do_groupingEv_nid_postfix(result, self);
}
APS5_EXPORT("aUNISsPboqE", sceLibcInternalMoneyPunctDoGroupingChar_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoCurrSymbolChar_nid_no_patch(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE14do_curr_symbolEv_nid_postfix(result, self);
}
APS5_EXPORT("VhyjwJugIe8", sceLibcInternalMoneyPunctDoCurrSymbolChar_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoPositiveSignChar_nid_no_patch(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE16do_positive_signEv_nid_postfix(result, self);
}
APS5_EXPORT("ARZszYKvDWg", sceLibcInternalMoneyPunctDoPositiveSignChar_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoNegativeSignChar_nid_no_patch(GuestLocale::String* result, const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE16do_negative_signEv_nid_postfix(result, self);
}
APS5_EXPORT("Rqu9OmkKY+M", sceLibcInternalMoneyPunctDoNegativeSignChar_nid_no_patch);

char APS5_VABI sceLibcInternalMoneyPunctDoFracDigitsChar_nid_no_patch(const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE14do_frac_digitsEv_nid_postfix(self);
}
APS5_EXPORT("C3LC9A6SrVE", sceLibcInternalMoneyPunctDoFracDigitsChar_nid_no_patch);

std::uint32_t APS5_VABI sceLibcInternalMoneyPunctDoPosFormatChar_nid_no_patch(const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE13do_pos_formatEv_nid_postfix(self);
}
APS5_EXPORT("E64hr8yXoXw", sceLibcInternalMoneyPunctDoPosFormatChar_nid_no_patch);

std::uint32_t APS5_VABI sceLibcInternalMoneyPunctDoNegFormatChar_nid_no_patch(const GuestLocale::MoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIcE13do_neg_formatEv_nid_postfix(self);
}
APS5_EXPORT("uUDq10y4Raw", sceLibcInternalMoneyPunctDoNegFormatChar_nid_no_patch);

std::uint16_t APS5_VABI sceLibcInternalMoneyPunctDoDecimalPointWide_nid_no_patch(const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE16do_decimal_pointEv_nid_postfix(self);
}
APS5_EXPORT("v8P1X84ytFY", sceLibcInternalMoneyPunctDoDecimalPointWide_nid_no_patch);

std::uint16_t APS5_VABI sceLibcInternalMoneyPunctDoThousandsSepWide_nid_no_patch(const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE16do_thousands_sepEv_nid_postfix(self);
}
APS5_EXPORT("XX+xiPXAN8I", sceLibcInternalMoneyPunctDoThousandsSepWide_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoGroupingWide_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE11do_groupingEv_nid_postfix(result, self);
}
APS5_EXPORT("ckD5sIxo+Co", sceLibcInternalMoneyPunctDoGroupingWide_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoCurrSymbolWide_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE14do_curr_symbolEv_nid_postfix(result, self);
}
APS5_EXPORT("ypq5jFNRIJA", sceLibcInternalMoneyPunctDoCurrSymbolWide_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoPositiveSignWide_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE16do_positive_signEv_nid_postfix(result, self);
}
APS5_EXPORT("PWFePkVdv9w", sceLibcInternalMoneyPunctDoPositiveSignWide_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalMoneyPunctDoNegativeSignWide_nid_no_patch(GuestLocale::String* result, const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE16do_negative_signEv_nid_postfix(result, self);
}
APS5_EXPORT("zkUC74aJxpE", sceLibcInternalMoneyPunctDoNegativeSignWide_nid_no_patch);

char APS5_VABI sceLibcInternalMoneyPunctDoFracDigitsWide_nid_no_patch(const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE14do_frac_digitsEv_nid_postfix(self);
}
APS5_EXPORT("ij-yZhH9YjY", sceLibcInternalMoneyPunctDoFracDigitsWide_nid_no_patch);

std::uint32_t APS5_VABI sceLibcInternalMoneyPunctDoPosFormatWide_nid_no_patch(const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE13do_pos_formatEv_nid_postfix(self);
}
APS5_EXPORT("zF2GfKzBgqU", sceLibcInternalMoneyPunctDoPosFormatWide_nid_no_patch);

std::uint32_t APS5_VABI sceLibcInternalMoneyPunctDoNegFormatWide_nid_no_patch(const GuestLocale::WideMoneyPunctFacet* self) {
    return _ZNKSt7_MpunctIwE13do_neg_formatEv_nid_postfix(self);
}
APS5_EXPORT("UzmR8lOfyqU", sceLibcInternalMoneyPunctDoNegFormatWide_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructWide_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self, unsigned long references, bool international) {
    _ZNSt7_MpunctIwEC2Emb_nid_postfix(self, references, international);
}
APS5_EXPORT("Pr1yLzUe230", sceLibcInternalMoneyPunctConstructWide_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyChar_nid_no_patch(GuestLocale::MoneyPunctFacet* self) {
    _ZNSt7_MpunctIcED0Ev_nid_postfix(self);
}
APS5_EXPORT("Lzj4ws7DlhQ", sceLibcInternalMoneyPunctDestroyChar_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyWide_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self) {
    _ZNSt7_MpunctIwED0Ev_nid_postfix(self);
}
APS5_EXPORT("8UeuxGKjQr8", sceLibcInternalMoneyPunctDestroyWide_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharNamed_nid_no_patch(GuestLocale::MoneyPunctFacet* self, const char* localeName, unsigned long references) {
    _ZNSt10moneypunctIcLb0EEC2EPKcm_nid_postfix(self, localeName, references);
}
APS5_EXPORT("aNfpdhcsMWI", sceLibcInternalMoneyPunctConstructCharNamed_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharChar_nid_no_patch(GuestLocale::MoneyPunctFacet* self, unsigned long references) {
    _ZNSt10moneypunctIcLb0EEC1Em_nid_postfix(self, references);
}
APS5_EXPORT("tYBLm0BoQdQ", sceLibcInternalMoneyPunctConstructCharChar_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharCharT_nid_no_patch(GuestLocale::MoneyPunctFacet* self, unsigned long references) {
    _ZNSt10moneypunctIcLb0EEC2Em_nid_postfix(self, references);
}
APS5_EXPORT("5DFeXjP+Plg", sceLibcInternalMoneyPunctConstructCharCharT_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharLocinfo_nid_no_patch(GuestLocale::MoneyPunctFacet* self, const GuestLocale::LocinfoStorage* info, unsigned long references, bool initialize) {
    _ZNSt10moneypunctIcLb0EEC1ERKSt8_Locinfomb_nid_postfix(self, info, references, initialize);
}
APS5_EXPORT("wrR3T5i7gpY", sceLibcInternalMoneyPunctConstructCharLocinfo_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharLocinfoT_nid_no_patch(GuestLocale::MoneyPunctFacet* self, const GuestLocale::LocinfoStorage* info, unsigned long references, bool initialize) {
    _ZNSt10moneypunctIcLb0EEC2ERKSt8_Locinfomb_nid_postfix(self, info, references, initialize);
}
APS5_EXPORT("uQFv8aNF8Jc", sceLibcInternalMoneyPunctConstructCharLocinfoT_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyCharDefault_nid_no_patch(GuestLocale::MoneyPunctFacet* self) {
    _ZNSt10moneypunctIcLb0EED0Ev_nid_postfix(self);
}
APS5_EXPORT("sS5fF+fht2c", sceLibcInternalMoneyPunctDestroyCharDefault_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharIntlNamed_nid_no_patch(GuestLocale::MoneyPunctFacet* self, const char* localeName, unsigned long references) {
    _ZNSt10moneypunctIcLb1EEC2EPKcm_nid_postfix(self, localeName, references);
}
APS5_EXPORT("RxJnJ-HoySc", sceLibcInternalMoneyPunctConstructCharIntlNamed_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharIntl_nid_no_patch(GuestLocale::MoneyPunctFacet* self, unsigned long references) {
    _ZNSt10moneypunctIcLb1EEC2Em_nid_postfix(self, references);
}
APS5_EXPORT("kPELiw9L-gw", sceLibcInternalMoneyPunctConstructCharIntl_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructCharIntlLocinfo_nid_no_patch(GuestLocale::MoneyPunctFacet* self, const GuestLocale::LocinfoStorage* info, unsigned long references, bool initialize) {
    _ZNSt10moneypunctIcLb1EEC2ERKSt8_Locinfomb_nid_postfix(self, info, references, initialize);
}
APS5_EXPORT("7e3DrnZea-Q", sceLibcInternalMoneyPunctConstructCharIntlLocinfo_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyCharIntlDefault_nid_no_patch(GuestLocale::MoneyPunctFacet* self) {
    _ZNSt10moneypunctIcLb1EED0Ev_nid_postfix(self);
}
APS5_EXPORT("tcdvTUlPnL0", sceLibcInternalMoneyPunctDestroyCharIntlDefault_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyCharIntl_nid_no_patch(GuestLocale::MoneyPunctFacet* self) {
    _ZNSt10moneypunctIcLb1EED1Ev_nid_postfix(self);
}
APS5_EXPORT("wT+HL7oqjYc", sceLibcInternalMoneyPunctDestroyCharIntl_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyCharIntlBase_nid_no_patch(GuestLocale::MoneyPunctFacet* self) {
    _ZNSt10moneypunctIcLb1EED2Ev_nid_postfix(self);
}
APS5_EXPORT("F7CUCpiasec", sceLibcInternalMoneyPunctDestroyCharIntlBase_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructWideNamed_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self, const char* localeName, unsigned long references) {
    _ZNSt10moneypunctIwLb0EEC1EPKcm_nid_postfix(self, localeName, references);
}
APS5_EXPORT("jna5sqISK4s", sceLibcInternalMoneyPunctConstructWideNamed_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructWideDefault_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self, unsigned long references) {
    _ZNSt10moneypunctIwLb0EEC1Em_nid_postfix(self, references);
}
APS5_EXPORT("McUBYCqjLMg", sceLibcInternalMoneyPunctConstructWideDefault_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctConstructWideIntlLocinfo_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self, const GuestLocale::LocinfoStorage* info, unsigned long references, bool initialize) {
    _ZNSt10moneypunctIwLb1EEC2ERKSt8_Locinfomb_nid_postfix(self, info, references, initialize);
}
APS5_EXPORT("H0a2QXvgHOk", sceLibcInternalMoneyPunctConstructWideIntlLocinfo_nid_no_patch);

void APS5_VABI sceLibcInternalMoneyPunctDestroyWideIntlDefault_nid_no_patch(GuestLocale::WideMoneyPunctFacet* self) {
    _ZNSt10moneypunctIwLb1EED0Ev_nid_postfix(self);
}
APS5_EXPORT("fWuQSVGOivA", sceLibcInternalMoneyPunctDestroyWideIntlDefault_nid_no_patch);

}

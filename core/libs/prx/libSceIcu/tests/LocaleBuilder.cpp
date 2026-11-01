// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    Icu::Abi::LocaleBuilder builder{};
    _ZN6icu_6813LocaleBuilderC1Ev_nid_postfix(&builder);
    Check(builder.status == 0 && builder.variant == nullptr && builder.extensions == nullptr);
    Check(_ZN6icu_6813LocaleBuilder11setLanguageENS_11StringPieceE_nid_postfix(&builder, {"AbCd", 4}) == &builder);
    _ZN6icu_6813LocaleBuilder9setScriptENS_11StringPieceE_nid_postfix(&builder, {"lAtN", 4});
    _ZN6icu_6813LocaleBuilder9setRegionENS_11StringPieceE_nid_postfix(&builder, {"419", 3});
    Check(std::strcmp(builder.language, "AbCd") == 0 && std::strcmp(builder.script, "lAtN") == 0 && std::strcmp(builder.region, "419") == 0);
    _ZN6icu_6813LocaleBuilder10setVariantENS_11StringPieceE_nid_postfix(&builder, {"POSIX_1234", 10});
    Check(builder.status == 0 && builder.variant && builder.variant->length == 10);
    Check(std::strcmp(builder.variant->buffer.pointer, "posix-1234") == 0);
    auto variant = builder.variant;
    _ZN6icu_6813LocaleBuilder10setVariantENS_11StringPieceE_nid_postfix(&builder, {"bad", 3});
    Check(builder.status == 1 && builder.variant == variant);
    _ZN6icu_6813LocaleBuilder11setLanguageENS_11StringPieceE_nid_postfix(&builder, {"en", 2});
    Check(std::strcmp(builder.language, "AbCd") == 0);
    std::int32_t status = 7;
    Check(_ZNK6icu_6813LocaleBuilder11copyErrorToER10UErrorCode_nid_postfix(&builder, &status) && status == 7);
    status = -1;
    Check(_ZNK6icu_6813LocaleBuilder11copyErrorToER10UErrorCode_nid_postfix(&builder, &status) && status == 1);
    Check(_ZN6icu_6813LocaleBuilder15clearExtensionsEv_nid_postfix(&builder) == &builder && builder.status == 1);
    Check(_ZN6icu_6813LocaleBuilder5clearEv_nid_postfix(&builder) == &builder && builder.status == 0 && builder.variant == nullptr);
    status = -1;
    Check(!_ZNK6icu_6813LocaleBuilder11copyErrorToER10UErrorCode_nid_postfix(&builder, &status) && status == 0);
    _ZN6icu_6813LocaleBuilder9setRegionENS_11StringPieceE_nid_postfix(&builder, {"ABC", 3});
    Check(builder.status == 1 && !builder.region[0]);
    _ZN6icu_6813LocaleBuilder5clearEv_nid_postfix(&builder);
    bool invalid = false;
    try { _ZN6icu_6813LocaleBuilder11setLanguageENS_11StringPieceE_nid_postfix(&builder, {"en", -1}); } catch (const std::invalid_argument&) { invalid = true; }
    Check(invalid);
    Check(_ZN6icu_6813LocaleBuilder9setLocaleERKNS_6LocaleE_nid_postfix(&builder, _ZN6icu_686Locale5getUSEv_nid_postfix()) == &builder);
    Check(builder.status == 0 && builder.extensions && !builder.variant);
    Check(std::strcmp(builder.language, "en") == 0 && std::strcmp(builder.region, "US") == 0);
    Check(std::strcmp(builder.extensions->fullName, "en_US") == 0);
    _ZN6icu_6813LocaleBuilderD1Ev_nid_postfix(&builder);
    Check(builder.vtable == Icu::Abi::Vtable_nid_no_patch(Icu::Abi::Class::Object));
    std::puts("ICU locale builder tests passed");
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WtfLettersLiteral.hpp"

#include <string_view>

namespace {

using WebCore::AutofillFieldName;
using namespace std::string_view_literals;

struct FieldNameMapping {
    std::string_view literal;
    AutofillFieldName name;
};

constexpr FieldNameMapping FieldNameMappings[] = {
    {"additional-name"sv, AutofillFieldName::AdditionalName},
    {"address-level1"sv, AutofillFieldName::AddressLevel1},
    {"address-level2"sv, AutofillFieldName::AddressLevel2},
    {"address-level3"sv, AutofillFieldName::AddressLevel3},
    {"address-level4"sv, AutofillFieldName::AddressLevel4},
    {"address-line1"sv, AutofillFieldName::AddressLine1},
    {"address-line2"sv, AutofillFieldName::AddressLine2},
    {"address-line3"sv, AutofillFieldName::AddressLine3},
    {"bday"sv, AutofillFieldName::Bday},
    {"bday-day"sv, AutofillFieldName::BdayDay},
    {"bday-month"sv, AutofillFieldName::BdayMonth},
    {"bday-year"sv, AutofillFieldName::BdayYear},
    {"cc-additional-name"sv, AutofillFieldName::CcAdditionalName},
    {"cc-csc"sv, AutofillFieldName::CcCsc},
    {"cc-exp"sv, AutofillFieldName::CcExp},
    {"cc-exp-month"sv, AutofillFieldName::CcExpMonth},
    {"cc-exp-year"sv, AutofillFieldName::CcExpYear},
    {"cc-family-name"sv, AutofillFieldName::CcFamilyName},
    {"cc-given-name"sv, AutofillFieldName::CcGivenName},
    {"cc-name"sv, AutofillFieldName::CcName},
    {"cc-number"sv, AutofillFieldName::CcNumber},
    {"cc-type"sv, AutofillFieldName::CcType},
    {"country"sv, AutofillFieldName::Country},
    {"country-name"sv, AutofillFieldName::CountryName},
    {"current-password"sv, AutofillFieldName::CurrentPassword},
    {"email"sv, AutofillFieldName::Email},
    {"family-name"sv, AutofillFieldName::FamilyName},
    {"given-name"sv, AutofillFieldName::GivenName},
    {"honorific-prefix"sv, AutofillFieldName::HonorificPrefix},
    {"honorific-suffix"sv, AutofillFieldName::HonorificSuffix},
    {"impp"sv, AutofillFieldName::Impp},
    {"language"sv, AutofillFieldName::Language},
    {"name"sv, AutofillFieldName::Name},
    {"new-password"sv, AutofillFieldName::NewPassword},
    {"nickname"sv, AutofillFieldName::Nickname},
    {"off"sv, AutofillFieldName::None},
    {"on"sv, AutofillFieldName::None},
    {"organization"sv, AutofillFieldName::Organization},
    {"organization-title"sv, AutofillFieldName::OrganizationTitle},
    {"photo"sv, AutofillFieldName::Photo},
    {"postal-code"sv, AutofillFieldName::PostalCode},
    {"sex"sv, AutofillFieldName::Sex},
    {"street-address"sv, AutofillFieldName::StreetAddress},
    {"tel"sv, AutofillFieldName::Tel},
    {"tel-area-code"sv, AutofillFieldName::TelAreaCode},
    {"tel-country-code"sv, AutofillFieldName::TelCountryCode},
    {"tel-extension"sv, AutofillFieldName::TelExtension},
    {"tel-local"sv, AutofillFieldName::TelLocal},
    {"tel-local-prefix"sv, AutofillFieldName::TelLocalPrefix},
    {"tel-local-suffix"sv, AutofillFieldName::TelLocalSuffix},
    {"tel-national"sv, AutofillFieldName::TelNational},
    {"transaction-amount"sv, AutofillFieldName::TransactionAmount},
    {"transaction-currency"sv, AutofillFieldName::TransactionCurrency},
    {"url"sv, AutofillFieldName::URL},
    {"username"sv, AutofillFieldName::Username},
    {"webauthn"sv, AutofillFieldName::WebAuthn},
};

}  // namespace

extern "C" {

AutofillFieldName APS5_VABI _ZN7WebCore19toAutofillFieldNameERKN3WTF10AtomStringE(const NKWebKit::WtfAbi::AtomString& value) {
    const auto* mapping = NKWebKit::WtfAbi::FindLettersIgnoringASCIICase(FieldNameMappings,
        [](const FieldNameMapping& entry) { return entry.literal; }, NKWebKit::WtfAbi::ViewOf(value.string.impl));
    return mapping ? mapping->name : AutofillFieldName::None;
}

}

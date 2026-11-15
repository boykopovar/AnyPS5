#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/CustomHeaderFieldsExports.hpp"

#include <new>
#include <string>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::CustomHeaderFields;

CustomHeaderFields* Checked(CustomHeaderFields* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::CustomHeaderFields::") + operation + ": null this");
    return self;
}

const CustomHeaderFields& Source(const CustomHeaderFields* other, const char* operation) {
    if (!other) throw std::invalid_argument(std::string("WebCore::CustomHeaderFields::") + operation + ": null source");
    return *other;
}

void ConstructEmpty(CustomHeaderFields* self) {
    new (Checked(self, "CustomHeaderFields")) CustomHeaderFields();
}

void ConstructCopy(CustomHeaderFields* self, const CustomHeaderFields* other) {
    const auto& source = Source(other, "CustomHeaderFields");
    new (Checked(self, "CustomHeaderFields")) CustomHeaderFields(source);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1Ev(CustomHeaderFields* self) {
    ConstructEmpty(self);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2Ev(CustomHeaderFields* self) {
    ConstructEmpty(self);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other);
}

CustomHeaderFields& APS5_VABI _ZN7WebCore18CustomHeaderFieldsaSERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    auto* fields = Checked(self, "operator=");
    const auto& source = Source(other, "operator=");
    if (fields != &source) {
        fields->fields = source.fields;
        fields->thirdPartyDomains = source.thirdPartyDomains;
    }
    return *fields;
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsD2Ev(CustomHeaderFields* self) {
    Checked(self, "~CustomHeaderFields")->~CustomHeaderFields();
}

}

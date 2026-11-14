#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKBooleanCreate(bool value) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Boolean>(value));
}

bool APS5_VABI WKBooleanGetValue(const void* booleanRef) {
    return NKWebKit::Cast<NKWebKit::Boolean>(booleanRef, __func__)->value;
}

const void* APS5_VABI WKDoubleCreate(double value) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Double>(value));
}

double APS5_VABI WKDoubleGetValue(const void* doubleRef) {
    return NKWebKit::Cast<NKWebKit::Double>(doubleRef, __func__)->value;
}

const void* APS5_VABI WKUInt64Create(std::uint64_t value) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::UInt64>(value));
}

std::uint64_t APS5_VABI WKUInt64GetValue(const void* uint64Ref) {
    return NKWebKit::Cast<NKWebKit::UInt64>(uint64Ref, __func__)->value;
}

}

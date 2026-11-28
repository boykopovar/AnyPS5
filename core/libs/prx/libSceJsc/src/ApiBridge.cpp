#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <stdexcept>

namespace {

std::atomic<const Jsc::ApiBridge*> registeredBridge{nullptr};

const Jsc::ApiBridge& Bridge(const char* function) {
    const auto* bridge = registeredBridge.load(std::memory_order_acquire);
    if (!bridge) NotImplemented_nid_no_patch(function);
    return *bridge;
}

template<typename Entry> Entry Bridged(Entry Jsc::ApiBridge::* member, const char* function) {
    const Entry entry = Bridge(function).*member;
    if (!entry) NotImplemented_nid_no_patch(function);
    return entry;
}

}

extern "C" {

void RegisterApiBridge_nid_no_patch(const Jsc::ApiBridge* bridge) {
    registeredBridge.store(bridge, std::memory_order_release);
}

const void* APS5_VABI JSValueMakeString(const void* context, const GuestJSString* string) {
    if (!context) return nullptr;
    return Bridge(__func__).makeString(context, string);
}

bool APS5_VABI JSObjectHasPropertyForKey(const void* context, const void* object, const void* key, const void** exception) {
    if (!context) return false;
    return Bridge(__func__).hasPropertyForKey(context, object, key, exception);
}

void APS5_VABI _ZN3WTF13StringBuilder6resizeEj(Jsc::WtfAbi::StringBuilder* self, unsigned newSize) {
    if (!self) throw std::invalid_argument(__func__);
    Bridge(__func__).resizeStringBuilder(self, newSize);
}

void APS5_VABI _ZN3WTF3URL24removeFragmentIdentifierEv(Jsc::WtfAbi::URL* self) {
    if (!self) throw std::invalid_argument(__func__);
    Bridged(&Jsc::ApiBridge::removeUrlFragmentIdentifier, __func__)(self);
}

void APS5_VABI _ZN3WTF11writeIndentERNS_10TextStreamEi(Jsc::WtfAbi::TextStream* stream, int indent) {
    if (!stream) throw std::invalid_argument(__func__);
    Bridged(&Jsc::ApiBridge::writeTextStreamIndent, __func__)(stream, indent);
}

Jsc::WtfAbi::TextStream* APS5_VABI _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Jsc::WtfAbi::TextStream* self, const Jsc::WtfAbi::FormatNumberRespectingIntegers* number) {
    if (!self || !number) throw std::invalid_argument(__func__);
    Bridged(&Jsc::ApiBridge::appendNumberRespectingIntegers, __func__)(self, number->value);
    return self;
}

void APS5_VABI _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Jsc::WtfAbi::URL* self, const Jsc::WtfAbi::URL* base, const Jsc::WtfAbi::String* relative, const void* encoding) {
    if (!self || !base || !relative) throw std::invalid_argument(__func__);
    Bridged(&Jsc::ApiBridge::constructUrl, __func__)(self, base, relative, encoding);
}

void APS5_VABI _ZN3WTF3URL11setPasswordENS_10StringViewE(Jsc::WtfAbi::URL* self, Jsc::WtfAbi::StringView password) {
    if (!self) throw std::invalid_argument(__func__);
    Bridged(&Jsc::ApiBridge::setUrlPassword, __func__)(self, password);
}

}

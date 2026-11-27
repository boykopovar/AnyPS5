#pragma once

#include "SceTypes.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/src/WtfTextStreamAbi.hpp"
#include "prx/libSceJsc/src/WtfUrlAbi.hpp"

namespace Jsc::WtfAbi {

struct String;

} // namespace Jsc::WtfAbi

namespace Jsc {

struct ApiBridge {
    const void* (*makeString)(const void* context, const GuestJSString* string);
    bool (*hasPropertyForKey)(const void* context, const void* object, const void* key, const void** exception);
    void (*resizeStringBuilder)(WtfAbi::StringBuilder* builder, unsigned newSize);
    void (*removeUrlFragmentIdentifier)(WtfAbi::URL* url);
    void (*writeTextStreamIndent)(WtfAbi::TextStream* stream, int indent);
    void (*appendNumberRespectingIntegers)(WtfAbi::TextStream* stream, double value);
    void (*constructUrl)(WtfAbi::URL* url, const WtfAbi::URL* base, const WtfAbi::String* relative, const void* encoding);
    void (*setUrlPassword)(WtfAbi::URL* url, WtfAbi::StringView password);
};

}

extern "C" {

const void* APS5_VABI JSValueMakeString(const void* context, const GuestJSString* string);
bool APS5_VABI JSObjectHasPropertyForKey(const void* context, const void* object, const void* key, const void** exception);
void APS5_VABI _ZN3WTF13StringBuilder6resizeEj(Jsc::WtfAbi::StringBuilder* self, unsigned newSize);
void APS5_VABI _ZN3WTF3URL24removeFragmentIdentifierEv(Jsc::WtfAbi::URL* self);
void APS5_VABI _ZN3WTF11writeIndentERNS_10TextStreamEi(Jsc::WtfAbi::TextStream* stream, int indent);
Jsc::WtfAbi::TextStream* APS5_VABI _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Jsc::WtfAbi::TextStream* self, const Jsc::WtfAbi::FormatNumberRespectingIntegers* number);
void APS5_VABI _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Jsc::WtfAbi::URL* self, const Jsc::WtfAbi::URL* base, const Jsc::WtfAbi::String* relative, const void* encoding);
void APS5_VABI _ZN3WTF3URL11setPasswordENS_10StringViewE(Jsc::WtfAbi::URL* self, Jsc::WtfAbi::StringView password);
void RegisterApiBridge_nid_no_patch(const Jsc::ApiBridge* bridge);

}

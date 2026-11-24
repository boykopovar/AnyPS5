#pragma once

#include "SceTypes.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/src/WtfTextStreamAbi.hpp"
#include "prx/libSceJsc/src/WtfUrlAbi.hpp"

namespace Jsc {

struct ApiBridge {
    const void* (*makeString)(const void* context, const GuestJSString* string);
    bool (*hasPropertyForKey)(const void* context, const void* object, const void* key, const void** exception);
    void (*resizeStringBuilder)(WtfAbi::StringBuilder* builder, unsigned newSize);
    void (*removeUrlFragmentIdentifier)(WtfAbi::URL* url);
    void (*writeTextStreamIndent)(WtfAbi::TextStream* stream, int indent);
    void (*appendNumberRespectingIntegers)(WtfAbi::TextStream* stream, double value);
};

}

extern "C" {

const void* APS5_VABI JSValueMakeString(const void* context, const GuestJSString* string);
bool APS5_VABI JSObjectHasPropertyForKey(const void* context, const void* object, const void* key, const void** exception);
void APS5_VABI _ZN3WTF13StringBuilder6resizeEj(Jsc::WtfAbi::StringBuilder* self, unsigned newSize);
void APS5_VABI _ZN3WTF3URL24removeFragmentIdentifierEv(Jsc::WtfAbi::URL* self);
void APS5_VABI _ZN3WTF11writeIndentERNS_10TextStreamEi(Jsc::WtfAbi::TextStream* stream, int indent);
Jsc::WtfAbi::TextStream* APS5_VABI _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Jsc::WtfAbi::TextStream* self, const Jsc::WtfAbi::FormatNumberRespectingIntegers* number);
void RegisterApiBridge_nid_no_patch(const Jsc::ApiBridge* bridge);

}

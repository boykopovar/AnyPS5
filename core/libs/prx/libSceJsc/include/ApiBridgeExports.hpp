#pragma once

#include "SceTypes.hpp"
#include "prx/libSceJsc/include/Api.hpp"

namespace Jsc {

struct ApiBridge {
    const void* (*makeString)(const void* context, const GuestJSString* string);
    bool (*hasPropertyForKey)(const void* context, const void* object, const void* key, const void** exception);
};

}

extern "C" {

const void* APS5_VABI JSValueMakeString(const void* context, const GuestJSString* string);
bool APS5_VABI JSObjectHasPropertyForKey(const void* context, const void* object, const void* key, const void** exception);
void RegisterApiBridge_nid_no_patch(const Jsc::ApiBridge* bridge);

}

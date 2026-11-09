#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/GuestLocale.hpp"

struct StreamFlagManipulator {
    void (APS5_VABI* function)(GuestLocale::IosBase*, std::int32_t);
    std::int32_t argument;
};

static_assert(offsetof(StreamFlagManipulator, function) == 0);
static_assert(offsetof(StreamFlagManipulator, argument) == 8);
static_assert(sizeof(StreamFlagManipulator) == 16);

extern "C" {

StreamFlagManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask);
StreamFlagManipulator APS5_VABI _ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask);

}

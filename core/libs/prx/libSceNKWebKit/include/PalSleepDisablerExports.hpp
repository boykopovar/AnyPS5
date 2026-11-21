#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace NKWebKit::PalAbi {

enum class SleepDisablerType : int {
    System,
    Display,
};

struct SleepDisablerVtable {
    void (APS5_VABI* completeDestructor)(void* self);
    void (APS5_VABI* deletingDestructor)(void* self);
};

struct SleepDisabler {
    const SleepDisablerVtable* vptr;
    SleepDisablerType type;
};

static_assert(sizeof(SleepDisablerVtable) == 16);
static_assert(sizeof(SleepDisabler) == 16);
static_assert(alignof(SleepDisabler) == 8);
static_assert(offsetof(SleepDisabler, type) == 8);

}  // namespace NKWebKit::PalAbi

extern "C" {

void APS5_VABI _ZN3PAL13SleepDisablerC1EPKcNS0_4TypeE(NKWebKit::PalAbi::SleepDisabler* self, const char* reason, NKWebKit::PalAbi::SleepDisablerType type);
void APS5_VABI _ZN3PAL13SleepDisablerC2EPKcNS0_4TypeE(NKWebKit::PalAbi::SleepDisabler* self, const char* reason, NKWebKit::PalAbi::SleepDisablerType type);
void APS5_VABI _ZN3PAL13SleepDisablerD0Ev(NKWebKit::PalAbi::SleepDisabler* self);
void** APS5_VABI _ZN7WebCore19sleepDisablerClientEv();

}

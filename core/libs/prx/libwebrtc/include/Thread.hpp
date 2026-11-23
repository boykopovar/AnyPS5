#pragma once

#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcAnyInvocableState {
    alignas(16) unsigned char storage[16];
};

struct WebRtcAnyInvocable {
    WebRtcAnyInvocableState state;
    void (APS5_VABI* manager)(std::uint8_t operation, WebRtcAnyInvocableState* from, WebRtcAnyInvocableState* to);
    void (APS5_VABI* invoker)(WebRtcAnyInvocableState* state);
};

static_assert(sizeof(WebRtcAnyInvocable) == 32);
static_assert(alignof(WebRtcAnyInvocable) == 16);
static_assert(offsetof(WebRtcAnyInvocable, manager) == 16);
static_assert(offsetof(WebRtcAnyInvocable, invoker) == 24);

extern "C" {

void* const* APS5_VABI WebRtcThreadVtable_nid_no_patch();

}

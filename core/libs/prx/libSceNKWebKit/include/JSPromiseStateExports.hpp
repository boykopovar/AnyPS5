#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/JSPromiseAbi.hpp"

#include <cstdint>

extern "C" {

std::uint32_t APS5_VABI _ZNK3JSC9JSPromise6statusERNS_2VME(const NKWebKit::JscAbi::JSPromise* self, void* vm);
std::uint64_t APS5_VABI _ZNK3JSC9JSPromise6resultERNS_2VME(const NKWebKit::JscAbi::JSPromise* self, void* vm);
bool APS5_VABI _ZNK3JSC9JSPromise9isHandledERNS_2VME(const NKWebKit::JscAbi::JSPromise* self, void* vm);

}

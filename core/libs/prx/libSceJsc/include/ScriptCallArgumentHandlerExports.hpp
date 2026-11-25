#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::ScriptCallAbi {

struct MarkedArgumentBuffer {
    std::uint8_t overflowed;
    std::int32_t size;
    std::int32_t capacity;
    std::uint64_t inlineBuffer[8];
    std::uint64_t* buffer;
    void* markSet;
};

static_assert(offsetof(MarkedArgumentBuffer, size) == 4);
static_assert(offsetof(MarkedArgumentBuffer, capacity) == 8);
static_assert(offsetof(MarkedArgumentBuffer, inlineBuffer) == 16);
static_assert(offsetof(MarkedArgumentBuffer, buffer) == 80);
static_assert(offsetof(MarkedArgumentBuffer, markSet) == 88);
static_assert(sizeof(MarkedArgumentBuffer) == 96);

struct ScriptCallArgumentHandler {
    MarkedArgumentBuffer arguments;
    void* globalObject;
};

static_assert(offsetof(ScriptCallArgumentHandler, globalObject) == 96);
static_assert(sizeof(ScriptCallArgumentHandler) == 104);

} // namespace Jsc::ScriptCallAbi

extern "C" {

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, void* globalObject);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, void* globalObject);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, std::uint64_t argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, bool argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, int argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, unsigned int argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, std::int64_t argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, std::uint64_t argument);
void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx(Jsc::ScriptCallAbi::ScriptCallArgumentHandler* self, std::int64_t argument);

}

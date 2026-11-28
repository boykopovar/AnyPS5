#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::JscAbi {

struct Register {
    std::int32_t payload;
    std::int32_t tag;
};

struct ProtoCallFrame {
    std::uint64_t codeBlockValue;
    std::uint64_t calleeValue;
    Register argCountAndCodeOriginValue;
    std::uint64_t thisArg;
    std::uint32_t paddedArgCount;
    bool hasArityMismatch;
    std::uint64_t* args;
    std::uint64_t globalObject;
};

static_assert(sizeof(Register) == 8);
static_assert(offsetof(Register, payload) == 0);
static_assert(offsetof(Register, tag) == 4);
static_assert(sizeof(ProtoCallFrame) == 56);
static_assert(alignof(ProtoCallFrame) == 8);
static_assert(offsetof(ProtoCallFrame, codeBlockValue) == 0);
static_assert(offsetof(ProtoCallFrame, calleeValue) == 8);
static_assert(offsetof(ProtoCallFrame, argCountAndCodeOriginValue) == 16);
static_assert(offsetof(ProtoCallFrame, thisArg) == 24);
static_assert(offsetof(ProtoCallFrame, paddedArgCount) == 32);
static_assert(offsetof(ProtoCallFrame, hasArityMismatch) == 36);
static_assert(offsetof(ProtoCallFrame, args) == 40);
static_assert(offsetof(ProtoCallFrame, globalObject) == 48);

} // namespace Jsc::JscAbi

extern "C" {

void* APS5_VABI _ZNK3JSC14ProtoCallFrame9codeBlockEv(const Jsc::JscAbi::ProtoCallFrame* self);
bool APS5_VABI _ZN3JSC14ProtoCallFrame14needArityCheckEv(Jsc::JscAbi::ProtoCallFrame* self);
void APS5_VABI _ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_(
    Jsc::JscAbi::ProtoCallFrame* self, void* codeBlock, void* globalObject, void* callee, std::uint64_t thisValue,
    int argCountIncludingThis, std::uint64_t* otherArgs);

}

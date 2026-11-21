#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscAbi {

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
    std::uint64_t* args;
    std::uint64_t globalObject;
};

static_assert(sizeof(Register) == 8);
static_assert(offsetof(Register, payload) == 0);
static_assert(offsetof(Register, tag) == 4);
static_assert(sizeof(ProtoCallFrame) == 56);
static_assert(offsetof(ProtoCallFrame, codeBlockValue) == 0);
static_assert(offsetof(ProtoCallFrame, calleeValue) == 8);
static_assert(offsetof(ProtoCallFrame, argCountAndCodeOriginValue) == 16);
static_assert(offsetof(ProtoCallFrame, thisArg) == 24);
static_assert(offsetof(ProtoCallFrame, paddedArgCount) == 32);
static_assert(offsetof(ProtoCallFrame, args) == 40);
static_assert(offsetof(ProtoCallFrame, globalObject) == 48);

}

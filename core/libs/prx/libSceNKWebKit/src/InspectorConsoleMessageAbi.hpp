#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::InspectorAbi {

using EncodedJSValue = std::uint64_t;
using HandleSlot = EncodedJSValue*;

struct JSONLogValue {
    int type;
    WtfAbi::String value;
};

struct ScriptArguments {
    unsigned refCount;
    HandleSlot globalObject;
    WtfAbi::Vector<HandleSlot> arguments;
};

struct ConsoleMessage {
    std::uint8_t source;
    std::uint8_t type;
    std::uint8_t level;
    WtfAbi::String message;
    ScriptArguments* arguments;
    void* callStack;
    WtfAbi::Vector<JSONLogValue> jsonLogValues;
    WtfAbi::String url;
    HandleSlot globalObject;
    unsigned line;
    unsigned column;
    unsigned repeatCount;
    WtfAbi::String requestId;
    double timestamp;
};

static_assert(sizeof(JSONLogValue) == 16);
static_assert(sizeof(WtfAbi::Vector<HandleSlot>) == 16);
static_assert(offsetof(ScriptArguments, refCount) == 0);
static_assert(offsetof(ScriptArguments, globalObject) == 8);
static_assert(offsetof(ScriptArguments, arguments) == 16);
static_assert(sizeof(ScriptArguments) == 32);
static_assert(offsetof(ConsoleMessage, source) == 0);
static_assert(offsetof(ConsoleMessage, type) == 1);
static_assert(offsetof(ConsoleMessage, level) == 2);
static_assert(offsetof(ConsoleMessage, message) == 8);
static_assert(offsetof(ConsoleMessage, arguments) == 16);
static_assert(offsetof(ConsoleMessage, callStack) == 24);
static_assert(offsetof(ConsoleMessage, jsonLogValues) == 32);
static_assert(offsetof(ConsoleMessage, url) == 48);
static_assert(offsetof(ConsoleMessage, globalObject) == 56);
static_assert(offsetof(ConsoleMessage, line) == 64);
static_assert(offsetof(ConsoleMessage, column) == 68);
static_assert(offsetof(ConsoleMessage, repeatCount) == 72);
static_assert(offsetof(ConsoleMessage, requestId) == 80);
static_assert(offsetof(ConsoleMessage, timestamp) == 88);
static_assert(sizeof(ConsoleMessage) == 96);

}

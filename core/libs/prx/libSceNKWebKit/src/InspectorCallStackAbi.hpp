#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::InspectorAbi {

struct AsyncStackTrace;

struct ScriptCallFrame {
    WtfAbi::String functionName;
    WtfAbi::String scriptName;
    WtfAbi::String preRedirectURL;
    std::uint32_t sourceID;
    unsigned lineNumber;
    unsigned column;
};

struct ScriptCallFrameVector {
    ScriptCallFrame* buffer;
    unsigned capacity;
    unsigned size;
};

struct ScriptCallStack {
    unsigned refCount;
    ScriptCallFrameVector frames;
    bool truncated;
    AsyncStackTrace* parentStackTrace;
};

enum class AsyncStackTraceState : std::uint8_t {
    Pending,
    Active,
    Dispatched,
    Canceled,
};

struct AsyncStackTrace {
    unsigned refCount;
    ScriptCallStack* callStack;
    AsyncStackTrace* parent;
    unsigned childCount;
    AsyncStackTraceState state;
    bool truncated;
    bool singleShot;
};

struct ScriptCallStackRef {
    ScriptCallStack* ptr;
    explicit ScriptCallStackRef(ScriptCallStack* value) : ptr(value) {}
    ScriptCallStackRef(ScriptCallStackRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    ScriptCallStackRef(const ScriptCallStackRef&) = delete;
    ScriptCallStackRef& operator=(const ScriptCallStackRef&) = delete;
    ~ScriptCallStackRef();
};

void Retain(ScriptCallStack* stack);
void Release(ScriptCallStack* stack);
void Retain(AsyncStackTrace* trace);
void Release(AsyncStackTrace* trace);
bool IsNative(const ScriptCallFrame& frame);
bool FramesEqual(const ScriptCallFrame& a, const ScriptCallFrame& b);

static_assert(sizeof(ScriptCallFrame) == 40);
static_assert(alignof(ScriptCallFrame) == 8);
static_assert(offsetof(ScriptCallFrame, functionName) == 0);
static_assert(offsetof(ScriptCallFrame, scriptName) == 8);
static_assert(offsetof(ScriptCallFrame, preRedirectURL) == 16);
static_assert(offsetof(ScriptCallFrame, sourceID) == 24);
static_assert(offsetof(ScriptCallFrame, lineNumber) == 28);
static_assert(offsetof(ScriptCallFrame, column) == 32);

static_assert(sizeof(ScriptCallFrameVector) == 16);
static_assert(offsetof(ScriptCallFrameVector, buffer) == 0);
static_assert(offsetof(ScriptCallFrameVector, capacity) == 8);
static_assert(offsetof(ScriptCallFrameVector, size) == 12);

static_assert(sizeof(ScriptCallStack) == 40);
static_assert(alignof(ScriptCallStack) == 8);
static_assert(offsetof(ScriptCallStack, refCount) == 0);
static_assert(offsetof(ScriptCallStack, frames) == 8);
static_assert(offsetof(ScriptCallStack, truncated) == 24);
static_assert(offsetof(ScriptCallStack, parentStackTrace) == 32);

static_assert(sizeof(AsyncStackTrace) == 32);
static_assert(alignof(AsyncStackTrace) == 8);
static_assert(offsetof(AsyncStackTrace, refCount) == 0);
static_assert(offsetof(AsyncStackTrace, callStack) == 8);
static_assert(offsetof(AsyncStackTrace, parent) == 16);
static_assert(offsetof(AsyncStackTrace, childCount) == 24);
static_assert(offsetof(AsyncStackTrace, state) == 28);
static_assert(offsetof(AsyncStackTrace, truncated) == 29);
static_assert(offsetof(AsyncStackTrace, singleShot) == 30);

static_assert(sizeof(AsyncStackTraceState) == 1);
static_assert(sizeof(ScriptCallStackRef) == 8);
static_assert(!std::is_trivially_destructible_v<ScriptCallStackRef>);

}

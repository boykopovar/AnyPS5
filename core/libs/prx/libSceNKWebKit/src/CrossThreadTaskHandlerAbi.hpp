#pragma once

#include "SceTypes.hpp"

#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"

#include <cstddef>

namespace NKWebKit::WtfAbi {

struct CrossThreadTaskHandler;
struct CrossThreadTaskHandlerState;

struct CrossThreadTaskHandlerVtable {
    void (APS5_VABI* completeDestructor)(CrossThreadTaskHandler* self);
    void (APS5_VABI* deletingDestructor)(CrossThreadTaskHandler* self);
};

struct CrossThreadTaskHandlerVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    CrossThreadTaskHandlerVtable dispatch;
};

struct CrossThreadTaskHandler {
    const CrossThreadTaskHandlerVtable* vtable;
    CrossThreadTaskHandlerState* state;
    unsigned char unused[96];
    VoidCallableWrapper* completionCallback;
};

static_assert(sizeof(CrossThreadTaskHandlerVtable) == 16);
static_assert(offsetof(CrossThreadTaskHandlerVtable, completeDestructor) == 0);
static_assert(offsetof(CrossThreadTaskHandlerVtable, deletingDestructor) == 8);
static_assert(offsetof(CrossThreadTaskHandlerVtableStorage, dispatch) == 16);
static_assert(sizeof(CrossThreadTaskHandler) == 120);
static_assert(offsetof(CrossThreadTaskHandler, vtable) == 0);
static_assert(offsetof(CrossThreadTaskHandler, state) == 8);
static_assert(offsetof(CrossThreadTaskHandler, unused) == 16);
static_assert(offsetof(CrossThreadTaskHandler, completionCallback) == 112);

}  // namespace NKWebKit::WtfAbi

#pragma once

#include "SceTypes.hpp"
#include <cstddef>

namespace NKWebKit::WtfAbi {

struct VoidCallableWrapper;

struct VoidCallableWrapperVtable {
    void (APS5_VABI* completeDestructor)(VoidCallableWrapper* self);
    void (APS5_VABI* deletingDestructor)(VoidCallableWrapper* self);
    void (APS5_VABI* call)(VoidCallableWrapper* self);
};

struct VoidCallableWrapper {
    const VoidCallableWrapperVtable* vtable;
};

struct VoidFunction {
    VoidCallableWrapper* callableWrapper;
};

static_assert(sizeof(VoidCallableWrapperVtable) == 24);
static_assert(offsetof(VoidCallableWrapperVtable, completeDestructor) == 0);
static_assert(offsetof(VoidCallableWrapperVtable, deletingDestructor) == 8);
static_assert(offsetof(VoidCallableWrapperVtable, call) == 16);
static_assert(sizeof(VoidCallableWrapper) == 8);
static_assert(sizeof(VoidFunction) == 8);
static_assert(offsetof(VoidFunction, callableWrapper) == 0);

}  // namespace NKWebKit::WtfAbi

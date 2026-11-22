#pragma once

#include "SceTypes.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct CrossOriginAccessControlCheckDisabler;

struct CrossOriginAccessControlCheckDisablerVtable {
    void (APS5_VABI *completeDestructor)(CrossOriginAccessControlCheckDisabler*);
    void (APS5_VABI *deletingDestructor)(CrossOriginAccessControlCheckDisabler*);
    bool (APS5_VABI *crossOriginAccessControlCheckEnabled)(const CrossOriginAccessControlCheckDisabler*);
};

struct CrossOriginAccessControlCheckDisablerVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    CrossOriginAccessControlCheckDisablerVtable dispatch;
};

struct CrossOriginAccessControlCheckDisabler {
    const CrossOriginAccessControlCheckDisablerVtable* vptr;
    bool accessControlCheckEnabled;
};

static_assert(sizeof(CrossOriginAccessControlCheckDisablerVtable) == 24);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtable, completeDestructor) == 0);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtable, deletingDestructor) == 8);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtable, crossOriginAccessControlCheckEnabled) == 16);
static_assert(sizeof(CrossOriginAccessControlCheckDisablerVtableStorage) == 40);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtableStorage, offsetToTop) == 0);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtableStorage, typeInfo) == 8);
static_assert(offsetof(CrossOriginAccessControlCheckDisablerVtableStorage, dispatch) == 16);
static_assert(sizeof(CrossOriginAccessControlCheckDisabler) == 16);
static_assert(offsetof(CrossOriginAccessControlCheckDisabler, vptr) == 0);
static_assert(offsetof(CrossOriginAccessControlCheckDisabler, accessControlCheckEnabled) == 8);

}

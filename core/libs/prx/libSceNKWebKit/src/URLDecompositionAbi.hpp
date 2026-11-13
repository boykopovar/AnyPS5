#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct URLDecomposition;

struct URLDecompositionVTable {
    void* completeDestructor;
    void* deletingDestructor;
    WtfAbi::URL (APS5_VABI* fullURL)(const URLDecomposition* self);
    void (APS5_VABI* setFullURL)(URLDecomposition* self, const WtfAbi::URL* url);
};

struct URLDecomposition {
    const URLDecompositionVTable* vtable;
};

static_assert(sizeof(URLDecompositionVTable) == 32);
static_assert(offsetof(URLDecompositionVTable, completeDestructor) == 0);
static_assert(offsetof(URLDecompositionVTable, deletingDestructor) == 8);
static_assert(offsetof(URLDecompositionVTable, fullURL) == 16);
static_assert(offsetof(URLDecompositionVTable, setFullURL) == 24);
static_assert(sizeof(URLDecomposition) == 8);
static_assert(offsetof(URLDecomposition, vtable) == 0);

}  // namespace NKWebKit::WebCoreAbi

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace NKWebKit::InterfaceAbi {

struct InterfaceBase {
    const void* const* vptr;
};

struct RefCountedInterfaceBase {
    const void* const* vptr;
    unsigned refCount;
};

static_assert(sizeof(InterfaceBase) == 8);
static_assert(sizeof(RefCountedInterfaceBase) == 16);
static_assert(offsetof(RefCountedInterfaceBase, refCount) == 8);

const void* const* BlobRegistryVtable();
const void* const* DatabaseProviderVtable();
const void* const* HeapAnalyzerVtable();
const void* const* MediaRecorderProviderVtable();
const void* const* SceneIntegrationClientVtable();

}  // namespace NKWebKit::InterfaceAbi

extern "C" {

void APS5_VABI _ZN7WebCore12BlobRegistryC2Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7WebCore12BlobRegistryC2ERKS0_(NKWebKit::InterfaceAbi::InterfaceBase* self, const NKWebKit::InterfaceAbi::InterfaceBase* other);
void APS5_VABI _ZN7WebCore12BlobRegistryD0Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7WebCore16DatabaseProviderC1Ev(NKWebKit::InterfaceAbi::RefCountedInterfaceBase* self);
void APS5_VABI _ZN7WebCore16DatabaseProviderC2Ev(NKWebKit::InterfaceAbi::RefCountedInterfaceBase* self);
void APS5_VABI _ZN7WebCore16DatabaseProviderD0Ev(NKWebKit::InterfaceAbi::RefCountedInterfaceBase* self);
void APS5_VABI _ZN3JSC12HeapAnalyzerC2Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN3JSC12HeapAnalyzerC2ERKS0_(NKWebKit::InterfaceAbi::InterfaceBase* self, const NKWebKit::InterfaceAbi::InterfaceBase* other);
void APS5_VABI _ZN3JSC12HeapAnalyzerD0Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN3JSC12HeapAnalyzerD1Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7WebCore21MediaRecorderProviderC1Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7WebCore21MediaRecorderProviderC2Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7WebCore21MediaRecorderProviderC1ERKS0_(NKWebKit::InterfaceAbi::InterfaceBase* self, const NKWebKit::InterfaceAbi::InterfaceBase* other);
void APS5_VABI _ZN7WebCore21MediaRecorderProviderC2ERKS0_(NKWebKit::InterfaceAbi::InterfaceBase* self, const NKWebKit::InterfaceAbi::InterfaceBase* other);
void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientC2Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientC2ERKS1_(NKWebKit::InterfaceAbi::InterfaceBase* self, const NKWebKit::InterfaceAbi::InterfaceBase* other);
void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD0Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD1Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);
void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD2Ev(NKWebKit::InterfaceAbi::InterfaceBase* self);

}

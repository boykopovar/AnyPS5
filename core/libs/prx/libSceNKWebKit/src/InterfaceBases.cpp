#include "prx/libSceNKWebKit/include/InterfaceBaseExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::InterfaceAbi::InterfaceBase;
using NKWebKit::InterfaceAbi::RefCountedInterfaceBase;

template<std::size_t SlotCount> struct Vtable {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    const void* slots[SlotCount];
};

template<typename T> T& Checked(T* object, const char* function) {
    if (!object) throw std::invalid_argument(std::string(function) + " requires an object");
    return *object;
}

template<typename Function> const void* Slot(Function function) {
    return reinterpret_cast<const void*>(function);
}

void APS5_VABI EmptyDestructor(InterfaceBase* self) {
    Checked(self, "interface destructor");
}

void APS5_VABI GlobalDeletingDestructor(InterfaceBase* self) {
    EmptyDestructor(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI FastDeletingDestructor(InterfaceBase* self) {
    EmptyDestructor(self);
    _ZN3WTF8fastFreeEPv(self);
}

const void* PureVirtual() {
    return Slot(&__cxa_pure_virtual_nid_postfix);
}

const Vtable<13> BlobRegistryTable = {0, nullptr, {
    PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(),
    PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(),
    Slot(&_ZN7WebCore12BlobRegistry16blobRegistryImplEv), Slot(&EmptyDestructor), Slot(&GlobalDeletingDestructor)}};

const Vtable<3> DatabaseProviderTable = {0, nullptr, {Slot(&EmptyDestructor), Slot(&FastDeletingDestructor), PureVirtual()}};

const Vtable<10> HeapAnalyzerTable = {0, nullptr, {
    Slot(&EmptyDestructor), Slot(&GlobalDeletingDestructor),
    PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual(), PureVirtual()}};

const Vtable<2> MediaRecorderProviderTable = {0, nullptr, {Slot(&EmptyDestructor), Slot(&FastDeletingDestructor)}};

const Vtable<3> SceneIntegrationClientTable = {0, nullptr, {Slot(&EmptyDestructor), Slot(&GlobalDeletingDestructor), PureVirtual()}};

void Install(InterfaceBase* self, const void* const* vtable, const char* function) {
    Checked(self, function).vptr = vtable;
}

void CopyInstall(InterfaceBase* self, const InterfaceBase* other, const void* const* vtable, const char* function) {
    Checked(other, function);
    Install(self, vtable, function);
}

void ConstructDatabaseProvider(RefCountedInterfaceBase* self) {
    auto& provider = Checked(self, "WebCore::DatabaseProvider::DatabaseProvider");
    provider.vptr = DatabaseProviderTable.slots;
    provider.refCount = 1;
}

}  // namespace

namespace NKWebKit::InterfaceAbi {

const void* const* BlobRegistryVtable() {
    return BlobRegistryTable.slots;
}

const void* const* DatabaseProviderVtable() {
    return DatabaseProviderTable.slots;
}

const void* const* HeapAnalyzerVtable() {
    return HeapAnalyzerTable.slots;
}

const void* const* MediaRecorderProviderVtable() {
    return MediaRecorderProviderTable.slots;
}

const void* const* SceneIntegrationClientVtable() {
    return SceneIntegrationClientTable.slots;
}

}  // namespace NKWebKit::InterfaceAbi

extern "C" {

void APS5_VABI _ZN7WebCore12BlobRegistryC2Ev(InterfaceBase* self) {
    Install(self, BlobRegistryTable.slots, "WebCore::BlobRegistry::BlobRegistry");
}

void APS5_VABI _ZN7WebCore12BlobRegistryC2ERKS0_(InterfaceBase* self, const InterfaceBase* other) {
    CopyInstall(self, other, BlobRegistryTable.slots, "WebCore::BlobRegistry::BlobRegistry");
}

void APS5_VABI _ZN7WebCore12BlobRegistryD0Ev(InterfaceBase* self) {
    GlobalDeletingDestructor(self);
}

void APS5_VABI _ZN7WebCore16DatabaseProviderC1Ev(RefCountedInterfaceBase* self) {
    ConstructDatabaseProvider(self);
}

void APS5_VABI _ZN7WebCore16DatabaseProviderC2Ev(RefCountedInterfaceBase* self) {
    ConstructDatabaseProvider(self);
}

void APS5_VABI _ZN7WebCore16DatabaseProviderD0Ev(RefCountedInterfaceBase* self) {
    FastDeletingDestructor(reinterpret_cast<InterfaceBase*>(self));
}

void APS5_VABI _ZN3JSC12HeapAnalyzerC2Ev(InterfaceBase* self) {
    Install(self, HeapAnalyzerTable.slots, "JSC::HeapAnalyzer::HeapAnalyzer");
}

void APS5_VABI _ZN3JSC12HeapAnalyzerC2ERKS0_(InterfaceBase* self, const InterfaceBase* other) {
    CopyInstall(self, other, HeapAnalyzerTable.slots, "JSC::HeapAnalyzer::HeapAnalyzer");
}

void APS5_VABI _ZN3JSC12HeapAnalyzerD0Ev(InterfaceBase* self) {
    GlobalDeletingDestructor(self);
}

void APS5_VABI _ZN3JSC12HeapAnalyzerD1Ev(InterfaceBase* self) {
    EmptyDestructor(self);
}

void APS5_VABI _ZN7WebCore21MediaRecorderProviderC1Ev(InterfaceBase* self) {
    Install(self, MediaRecorderProviderTable.slots, "WebCore::MediaRecorderProvider::MediaRecorderProvider");
}

void APS5_VABI _ZN7WebCore21MediaRecorderProviderC2Ev(InterfaceBase* self) {
    Install(self, MediaRecorderProviderTable.slots, "WebCore::MediaRecorderProvider::MediaRecorderProvider");
}

void APS5_VABI _ZN7WebCore21MediaRecorderProviderC1ERKS0_(InterfaceBase* self, const InterfaceBase* other) {
    CopyInstall(self, other, MediaRecorderProviderTable.slots, "WebCore::MediaRecorderProvider::MediaRecorderProvider");
}

void APS5_VABI _ZN7WebCore21MediaRecorderProviderC2ERKS0_(InterfaceBase* self, const InterfaceBase* other) {
    CopyInstall(self, other, MediaRecorderProviderTable.slots, "WebCore::MediaRecorderProvider::MediaRecorderProvider");
}

void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientC2Ev(InterfaceBase* self) {
    Install(self, SceneIntegrationClientTable.slots, "Nicosia::SceneIntegration::Client::Client");
}

void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientC2ERKS1_(InterfaceBase* self, const InterfaceBase* other) {
    CopyInstall(self, other, SceneIntegrationClientTable.slots, "Nicosia::SceneIntegration::Client::Client");
}

void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD0Ev(InterfaceBase* self) {
    GlobalDeletingDestructor(self);
}

void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD1Ev(InterfaceBase* self) {
    EmptyDestructor(self);
}

void APS5_VABI _ZN7Nicosia16SceneIntegration6ClientD2Ev(InterfaceBase* self) {
    EmptyDestructor(self);
}

}

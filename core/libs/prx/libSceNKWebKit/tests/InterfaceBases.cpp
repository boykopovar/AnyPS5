#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InterfaceBaseExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <unordered_set>

using NKWebKit::InterfaceAbi::InterfaceBase;
using NKWebKit::InterfaceAbi::RefCountedInterfaceBase;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "InterfaceBases check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::unordered_set<void*> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = Allocate_nid_no_patch(size);
    return *pointer ? 0 : 12;
}

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using Destructor = void (APS5_VABI*)(void*);

static const void* PureVirtual() {
    return reinterpret_cast<const void*>(&__cxa_pure_virtual_nid_postfix);
}

static void RequireHeader(const void* const* vtable) {
    Require(reinterpret_cast<const std::ptrdiff_t*>(vtable)[-2] == 0 && vtable[-1] == nullptr);
}

static void RequirePure(const void* const* vtable, std::size_t first, std::size_t count) {
    for (std::size_t i = first; i < first + count; ++i) Require(vtable[i] == PureVirtual());
}

static Destructor SlotAt(const void* const* vtable, std::size_t index) {
    return reinterpret_cast<Destructor>(const_cast<void*>(vtable[index]));
}

static void TestBlobRegistry() {
    InterfaceBase registry{nullptr};
    _ZN7WebCore12BlobRegistryC2Ev(&registry);
    const auto* vtable = registry.vptr;
    Require(vtable == NKWebKit::InterfaceAbi::BlobRegistryVtable());
    RequireHeader(vtable);
    RequirePure(vtable, 0, 10);
    Require(vtable[10] == reinterpret_cast<const void*>(&_ZN7WebCore12BlobRegistry16blobRegistryImplEv));
    InterfaceBase copy{nullptr};
    _ZN7WebCore12BlobRegistryC2ERKS0_(&copy, &registry);
    Require(copy.vptr == vtable);

    auto* heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN7WebCore12BlobRegistryC2Ev(heap);
    SlotAt(vtable, 12)(heap);
    Require(guestAllocations.empty());
    heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN7WebCore12BlobRegistryC2Ev(heap);
    SlotAt(vtable, 11)(heap);
    _ZN7WebCore12BlobRegistryD0Ev(heap);
    Require(guestAllocations.empty());
}

static void TestDatabaseProvider() {
    RefCountedInterfaceBase provider{nullptr, 7};
    _ZN7WebCore16DatabaseProviderC1Ev(&provider);
    Require(provider.vptr == NKWebKit::InterfaceAbi::DatabaseProviderVtable() && provider.refCount == 1);
    RefCountedInterfaceBase base{nullptr, 9};
    _ZN7WebCore16DatabaseProviderC2Ev(&base);
    Require(base.vptr == provider.vptr && base.refCount == 1);
    RequireHeader(provider.vptr);
    Require(provider.vptr[2] == PureVirtual());

    auto* heap = static_cast<RefCountedInterfaceBase*>(_ZN3WTF10fastMallocEm(sizeof(RefCountedInterfaceBase)));
    _ZN7WebCore16DatabaseProviderC2Ev(heap);
    SlotAt(heap->vptr, 0)(heap);
    _ZN7WebCore16DatabaseProviderD0Ev(heap);
    heap = static_cast<RefCountedInterfaceBase*>(_ZN3WTF10fastMallocEm(sizeof(RefCountedInterfaceBase)));
    _ZN7WebCore16DatabaseProviderC1Ev(heap);
    SlotAt(heap->vptr, 1)(heap);
}

static void TestHeapAnalyzer() {
    InterfaceBase analyzer{nullptr};
    _ZN3JSC12HeapAnalyzerC2Ev(&analyzer);
    const auto* vtable = analyzer.vptr;
    Require(vtable == NKWebKit::InterfaceAbi::HeapAnalyzerVtable());
    RequireHeader(vtable);
    RequirePure(vtable, 2, 8);
    InterfaceBase copy{nullptr};
    _ZN3JSC12HeapAnalyzerC2ERKS0_(&copy, &analyzer);
    Require(copy.vptr == vtable);
    _ZN3JSC12HeapAnalyzerD1Ev(&copy);

    auto* heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN3JSC12HeapAnalyzerC2Ev(heap);
    SlotAt(vtable, 0)(heap);
    SlotAt(vtable, 1)(heap);
    Require(guestAllocations.empty());
    heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN3JSC12HeapAnalyzerC2Ev(heap);
    _ZN3JSC12HeapAnalyzerD0Ev(heap);
    Require(guestAllocations.empty());
}

static void TestMediaRecorderProvider() {
    InterfaceBase provider{nullptr};
    _ZN7WebCore21MediaRecorderProviderC1Ev(&provider);
    const auto* vtable = provider.vptr;
    Require(vtable == NKWebKit::InterfaceAbi::MediaRecorderProviderVtable());
    RequireHeader(vtable);
    InterfaceBase other{nullptr};
    _ZN7WebCore21MediaRecorderProviderC2Ev(&other);
    Require(other.vptr == vtable);
    _ZN7WebCore21MediaRecorderProviderC1ERKS0_(&other, &provider);
    Require(other.vptr == vtable);
    other.vptr = nullptr;
    _ZN7WebCore21MediaRecorderProviderC2ERKS0_(&other, &provider);
    Require(other.vptr == vtable);

    auto* heap = static_cast<InterfaceBase*>(_ZN3WTF10fastMallocEm(sizeof(InterfaceBase)));
    _ZN7WebCore21MediaRecorderProviderC2Ev(heap);
    SlotAt(vtable, 0)(heap);
    SlotAt(vtable, 1)(heap);
}

static void TestSceneIntegrationClient() {
    InterfaceBase client{nullptr};
    _ZN7Nicosia16SceneIntegration6ClientC2Ev(&client);
    const auto* vtable = client.vptr;
    Require(vtable == NKWebKit::InterfaceAbi::SceneIntegrationClientVtable());
    RequireHeader(vtable);
    Require(vtable[2] == PureVirtual());
    InterfaceBase copy{nullptr};
    _ZN7Nicosia16SceneIntegration6ClientC2ERKS1_(&copy, &client);
    Require(copy.vptr == vtable);
    _ZN7Nicosia16SceneIntegration6ClientD1Ev(&copy);
    _ZN7Nicosia16SceneIntegration6ClientD2Ev(&copy);

    auto* heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN7Nicosia16SceneIntegration6ClientC2Ev(heap);
    SlotAt(vtable, 1)(heap);
    Require(guestAllocations.empty());
    heap = static_cast<InterfaceBase*>(_Znwm_nid_postfix(sizeof(InterfaceBase)));
    _ZN7Nicosia16SceneIntegration6ClientC2Ev(heap);
    _ZN7Nicosia16SceneIntegration6ClientD0Ev(heap);
    Require(guestAllocations.empty());
}

static void TestRefusals() {
    InterfaceBase object{nullptr};
    Require(ThrowsInvalidArgument([] { _ZN7WebCore12BlobRegistryC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore12BlobRegistryC2ERKS0_(&object, nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore12BlobRegistryD0Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore16DatabaseProviderC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore16DatabaseProviderD0Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3JSC12HeapAnalyzerC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3JSC12HeapAnalyzerD1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore21MediaRecorderProviderC1ERKS0_(nullptr, &object); }));
    Require(ThrowsInvalidArgument([] { _ZN7Nicosia16SceneIntegration6ClientD2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7Nicosia16SceneIntegration6ClientC2ERKS1_(&object, nullptr); }));
    Require(!object.vptr);
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestBlobRegistry();
    TestDatabaseProvider();
    TestHeapAnalyzer();
    TestMediaRecorderProvider();
    TestSceneIntegrationClient();
    TestRefusals();
    return 0;
}

#include "prx/libSceNKWebKit/include/VtableConstructorExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <utility>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "VtableConstructors check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

alignas(16) static unsigned char object[0x1000];

static void Fill() {
    std::memset(object, 0xa5, sizeof(object));
}

static const void* const* Vptr() {
    const void* const* value;
    std::memcpy(&value, object, sizeof(value));
    return value;
}

static std::uint64_t Value(std::size_t offset, std::size_t size) {
    std::uint64_t value = 0;
    std::memcpy(&value, object + offset, size);
    return value;
}

static bool AllBytes(std::size_t offset, std::size_t size, unsigned char byte) {
    for (std::size_t i = 0; i < size; ++i)
        if (object[offset + i] != byte) return false;
    return true;
}

static bool Untouched(std::initializer_list<std::pair<std::size_t, std::size_t>> written) {
    for (std::size_t i = 0; i < sizeof(object); ++i) {
        bool inside = false;
        for (const auto& range : written) inside = inside || (i >= range.first && i < range.first + range.second);
        if (!inside && object[i] != 0xa5) return false;
    }
    return true;
}

static const void* Pure() {
    return NKWebKitVtableSlotTargets_nid_no_patch()[0];
}

static int Count(const void* const* table, int slots, const void* target) {
    int count = 0;
    for (int i = 0; i < slots; ++i) count += table[2 + i] == target;
    return count;
}

template<typename F> static bool Throws(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    unsigned char result[16];
    std::memset(result, 0xa5, sizeof(result));
    Require(NKWebKitVtableNullResult_nid_no_patch(result) == result && result[7] == 0 && result[8] == 0xa5);
    Require(NKWebKitVtableZeroResult_nid_no_patch(result) == result && result[15] == 0);
    Require(NKWebKitVtableZero_nid_no_patch() == 0);
    NKWebKitVtableEmpty_nid_no_patch();
    Require(Count(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 1);
    Require(Count(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch(), 4, Pure()) == 0);
    Require(NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch()[5] != nullptr);
    Fill();
    _ZN3JSC15WeakHandleOwnerC1ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN3JSC15WeakHandleOwnerC1ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN3JSC15WeakHandleOwnerC1Ev(object);
    Require(Vptr() == NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN3JSC15WeakHandleOwnerC1Ev(nullptr); }));
    Fill();
    _ZN3JSC15WeakHandleOwnerC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN3JSC15WeakHandleOwnerC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN3JSC15WeakHandleOwnerC2Ev(object);
    Require(Vptr() == NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN3JSC15WeakHandleOwnerC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch(), 5, Pure()) == 3);
    Require(NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch()[6] != nullptr);
    Fill();
    _ZN3JSC20TypedArrayControllerC2Ev(object);
    Require(Vptr() == NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(Untouched({{0x0, 8}, {0x8, 4}}));
    Require(Throws([] { _ZN3JSC20TypedArrayControllerC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch(), 3, Pure()) == 1);
    Require(NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch()[4] != nullptr);
    Fill();
    _ZN3WTF18FunctionDispatcherC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(Untouched({{0x0, 8}, {0x8, 4}}));
    Require(Throws([] { _ZN3WTF18FunctionDispatcherC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch(), 9, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 1);
    Require(Count(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch(), 9, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 1);
    Require(Count(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch(), 9, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch(), 9, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch(), 9, Pure()) == 0);
    Require(NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch()[10] != nullptr);
    Fill();
    _ZN7WebCore13MIMETypeCacheC1Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch() + 2);
    Require(Value(0x8, 1) == 0x0u);
    Require(Value(0x10, 1) == 0x0u);
    Require(Value(0x18, 1) == 0x0u);
    Require(Value(0x20, 1) == 0x0u);
    Require(Untouched({{0x0, 8}, {0x8, 1}, {0x10, 1}, {0x18, 1}, {0x20, 1}}));
    Require(Throws([] { _ZN7WebCore13MIMETypeCacheC1Ev(nullptr); }));
    Fill();
    _ZN7WebCore13MIMETypeCacheC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch() + 2);
    Require(Value(0x8, 1) == 0x0u);
    Require(Value(0x10, 1) == 0x0u);
    Require(Value(0x18, 1) == 0x0u);
    Require(Value(0x20, 1) == 0x0u);
    Require(Untouched({{0x0, 8}, {0x8, 1}, {0x10, 1}, {0x18, 1}, {0x20, 1}}));
    Require(Throws([] { _ZN7WebCore13MIMETypeCacheC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch(), 3, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch(), 3, Pure()) == 1);
    Require(NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch()[4] != nullptr);
    Fill();
    _ZN7WebCore15XPathNSResolverC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(Untouched({{0x0, 8}, {0x8, 4}}));
    Require(Throws([] { _ZN7WebCore15XPathNSResolverC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch(), 2, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch(), 2, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch(), 2, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch(), 2, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch(), 2, Pure()) == 0);
    Require(NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch()[3] != nullptr);
    Fill();
    _ZN7WebCore16BlobRegistryImplC1Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch() + 2);
    Require(AllBytes(0x8, 16, 0));
    Require(Untouched({{0x0, 8}, {0x8, 16}}));
    Require(Throws([] { _ZN7WebCore16BlobRegistryImplC1Ev(nullptr); }));
    Fill();
    _ZN7WebCore16BlobRegistryImplC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch() + 2);
    Require(AllBytes(0x8, 16, 0));
    Require(Untouched({{0x0, 8}, {0x8, 16}}));
    Require(Throws([] { _ZN7WebCore16BlobRegistryImplC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch(), 4, Pure()) == 2);
    Require(NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch()[5] != nullptr);
    Fill();
    _ZN7WebCore16VisitedLinkStoreC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(Value(0x10, 8) == 0x0u);
    Require(Untouched({{0x0, 8}, {0x8, 4}, {0x10, 8}}));
    Require(Throws([] { _ZN7WebCore16VisitedLinkStoreC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 1);
    Require(Count(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 1);
    Require(Count(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch(), 135, Pure()) == 97);
    Require(NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch()[136] != nullptr);
    Fill();
    _ZN7WebCore17FrameLoaderClientC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17FrameLoaderClientC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore17FrameLoaderClientC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17FrameLoaderClientC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 1);
    Require(Count(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch(), 4, Pure()) == 0);
    Require(NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch()[5] != nullptr);
    Fill();
    _ZN7WebCore17LibWebRTCProviderC1ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17LibWebRTCProviderC1ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore17LibWebRTCProviderC1Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17LibWebRTCProviderC1Ev(nullptr); }));
    Fill();
    _ZN7WebCore17LibWebRTCProviderC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17LibWebRTCProviderC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore17LibWebRTCProviderC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore17LibWebRTCProviderC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch(), 5, Pure()) == 3);
    Require(NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch()[6] != nullptr);
    Fill();
    _ZN7WebCore18PluginInfoProviderC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(Value(0x10, 8) == 0x0u);
    Require(Untouched({{0x0, 8}, {0x8, 4}, {0x10, 8}}));
    Require(Throws([] { _ZN7WebCore18PluginInfoProviderC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch(), 4, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch(), 4, Pure()) == 2);
    Require(NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch()[5] != nullptr);
    Fill();
    _ZN7WebCore19UserContentProviderC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(AllBytes(0x10, 16, 0));
    Require(Untouched({{0x0, 8}, {0x8, 4}, {0x10, 16}}));
    Require(Throws([] { _ZN7WebCore19UserContentProviderC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 11);
    Require(Count(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 3);
    Require(Count(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch(), 135, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch(), 135, Pure()) == 0);
    Require(NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch()[136] != nullptr);
    Fill();
    _ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore22EmptyFrameLoaderClientC1Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClientC1Ev(nullptr); }));
    Fill();
    _ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore22EmptyFrameLoaderClientC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore22EmptyFrameLoaderClientC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch(), 5, Pure()) == 3);
    Require(NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch()[6] != nullptr);
    Fill();
    _ZN7WebCore24StorageNamespaceProviderC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x1u);
    Require(AllBytes(0x10, 16, 0));
    Require(Untouched({{0x0, 8}, {0x8, 4}, {0x10, 16}}));
    Require(Throws([] { _ZN7WebCore24StorageNamespaceProviderC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch(), 6, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch(), 6, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch(), 6, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch(), 6, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch(), 6, Pure()) == 3);
    Require(NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch()[7] != nullptr);
    Fill();
    _ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore27ContentSecurityPolicyClientC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore27ContentSecurityPolicyClientC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch(), 5, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch(), 5, Pure()) == 0);
    Require(NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch()[6] != nullptr);
    Fill();
    _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev(nullptr); }));
    Fill();
    _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_(nullptr, nullptr); }));
    Fill();
    _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev(object);
    Require(Vptr() == NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch(), 8, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 1);
    Require(Count(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch(), 8, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch(), 8, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch(), 8, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch(), 8, Pure()) == 3);
    Require(NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch()[9] != nullptr);
    Fill();
    _ZN9Inspector15RemoteInspector6ClientC2ERKS1_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN9Inspector15RemoteInspector6ClientC2ERKS1_(nullptr, nullptr); }));
    Fill();
    _ZN9Inspector15RemoteInspector6ClientC2Ev(object);
    Require(Vptr() == NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch() + 2);
    Require(Untouched({{0x0, 8}}));
    Require(Throws([] { _ZN9Inspector15RemoteInspector6ClientC2Ev(nullptr); }));
    Require(Count(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch(), 7, NKWebKitVtableSlotTargets_nid_no_patch()[1]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch(), 7, NKWebKitVtableSlotTargets_nid_no_patch()[2]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch(), 7, NKWebKitVtableSlotTargets_nid_no_patch()[3]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch(), 7, NKWebKitVtableSlotTargets_nid_no_patch()[4]) == 0);
    Require(Count(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch(), 7, Pure()) == 5);
    Require(NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch()[0] == nullptr && NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch()[1] == nullptr && NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch()[8] != nullptr);
    Fill();
    _ZN9Inspector24RemoteControllableTargetC2ERKS0_(object, object + 0x800);
    Require(Vptr() == NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch() + 2);
    Require(std::memcmp(object + 0x8, object + 0x800 + 0x8, 4) == 0);
    Require(Untouched({{0x0, 8}, {0x8, 4}}));
    Require(Throws([] { _ZN9Inspector24RemoteControllableTargetC2ERKS0_(nullptr, nullptr); }));
    Fill();
    _ZN9Inspector24RemoteControllableTargetC2Ev(object);
    Require(Vptr() == NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch() + 2);
    Require(Value(0x8, 4) == 0x0u);
    Require(Untouched({{0x0, 8}, {0x8, 4}}));
    Require(Throws([] { _ZN9Inspector24RemoteControllableTargetC2Ev(nullptr); }));
    return 0;
}

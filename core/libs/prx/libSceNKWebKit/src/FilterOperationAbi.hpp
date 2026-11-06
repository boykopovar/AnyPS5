#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::FilterAbi {

struct SRGBAf {
    float components[4];
};

enum class CompositeOperation : std::uint8_t {
    Replace,
    Add,
    Accumulate,
};

enum class IterationCompositeOperation : std::uint8_t {
    Replace,
    Accumulate,
};

struct BlendingContext {
    double progress{0};
    bool isDiscrete{false};
    CompositeOperation compositeOperation{CompositeOperation::Replace};
    IterationCompositeOperation iterationCompositeOperation{IterationCompositeOperation::Replace};
    double currentIteration{0};
};

struct IntOutsets {
    int m_sides[4];
};

struct FilterOperations {
    WebCore::FilterOperation** buffer;
    unsigned capacity;
    unsigned size;
};

struct FilterOperationVtable;
class FilterOperationRef;
class FilterOperationPtr;

inline void RefOperation(WebCore::FilterOperation* operation) {
    if (operation) operation->m_refCount.fetch_add(1);
}

inline void DerefOperation(WebCore::FilterOperation* operation);

class FilterOperationRef {
public:
    explicit FilterOperationRef(WebCore::FilterOperation* operation) : m_operation(operation) {}
    FilterOperationRef(FilterOperationRef&& other) noexcept : m_operation(std::exchange(other.m_operation, nullptr)) {}
    FilterOperationRef(const FilterOperationRef&) = delete;
    FilterOperationRef& operator=(const FilterOperationRef&) = delete;
    FilterOperationRef& operator=(FilterOperationRef&&) = delete;
    ~FilterOperationRef() { DerefOperation(m_operation); }

    WebCore::FilterOperation* Get() const { return m_operation; }
    WebCore::FilterOperation* Leak() { return std::exchange(m_operation, nullptr); }

private:
    WebCore::FilterOperation* m_operation;
};

class FilterOperationPtr {
public:
    FilterOperationPtr() = default;
    explicit FilterOperationPtr(WebCore::FilterOperation* operation) : m_operation(operation) { RefOperation(m_operation); }
    FilterOperationPtr(const FilterOperationPtr& other) : m_operation(other.m_operation) { RefOperation(m_operation); }
    FilterOperationPtr(FilterOperationPtr&& other) noexcept : m_operation(std::exchange(other.m_operation, nullptr)) {}
    FilterOperationPtr(FilterOperationRef&& other) noexcept : m_operation(other.Leak()) {}
    FilterOperationPtr& operator=(const FilterOperationPtr&) = delete;
    FilterOperationPtr& operator=(FilterOperationPtr&&) = delete;
    ~FilterOperationPtr() { DerefOperation(m_operation); }

    WebCore::FilterOperation* Get() const { return m_operation; }
    WebCore::FilterOperation* Leak() { return std::exchange(m_operation, nullptr); }

private:
    WebCore::FilterOperation* m_operation{nullptr};
};

struct FilterOperationVtable {
    void (APS5_VABI* completeDestructor)(WebCore::FilterOperation*);
    void (APS5_VABI* deletingDestructor)(WebCore::FilterOperation*);
    FilterOperationRef (APS5_VABI* clone)(const WebCore::FilterOperation*);
    bool (APS5_VABI* equals)(const WebCore::FilterOperation*, const WebCore::FilterOperation*);
    FilterOperationPtr (APS5_VABI* blend)(WebCore::FilterOperation*, const WebCore::FilterOperation*, const BlendingContext*, bool);
    bool (APS5_VABI* transformColor)(const WebCore::FilterOperation*, SRGBAf*);
    bool (APS5_VABI* inverseTransformColor)(const WebCore::FilterOperation*, SRGBAf*);
    bool (APS5_VABI* isIdentity)(const WebCore::FilterOperation*);
    IntOutsets (APS5_VABI* outsets)(const WebCore::FilterOperation*);
    bool (APS5_VABI* affectsOpacity)(const WebCore::FilterOperation*);
    bool (APS5_VABI* movesPixels)(const WebCore::FilterOperation*);
    bool (APS5_VABI* shouldBeRestrictedBySecurityOrigin)(const WebCore::FilterOperation*);
};

struct FilterOperationVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    FilterOperationVtable dispatch;
};

inline void DerefOperation(WebCore::FilterOperation* operation) {
    if (!operation) return;
    if (operation->m_refCount.fetch_sub(1) != 1) return;
    static_cast<const FilterOperationVtable*>(operation->m_vptr)->deletingDestructor(operation);
}

static_assert(sizeof(SRGBAf) == 16);
static_assert(alignof(SRGBAf) == 4);
static_assert(sizeof(CompositeOperation) == 1);
static_assert(sizeof(IterationCompositeOperation) == 1);
static_assert(sizeof(BlendingContext) == 24);
static_assert(offsetof(BlendingContext, progress) == 0);
static_assert(offsetof(BlendingContext, isDiscrete) == 8);
static_assert(offsetof(BlendingContext, compositeOperation) == 9);
static_assert(offsetof(BlendingContext, iterationCompositeOperation) == 10);
static_assert(offsetof(BlendingContext, currentIteration) == 16);
static_assert(sizeof(IntOutsets) == 16);
static_assert(alignof(IntOutsets) == 4);
static_assert(std::is_trivially_copyable_v<IntOutsets>);
static_assert(sizeof(FilterOperations) == sizeof(NKWebKit::WtfAbi::Vector<WebCore::FilterOperation*>));
static_assert(offsetof(FilterOperations, buffer) == 0);
static_assert(offsetof(FilterOperations, capacity) == 8);
static_assert(offsetof(FilterOperations, size) == 12);
static_assert(sizeof(FilterOperationVtable) == 96);
static_assert(offsetof(FilterOperationVtable, completeDestructor) == 0);
static_assert(offsetof(FilterOperationVtable, deletingDestructor) == 8);
static_assert(offsetof(FilterOperationVtable, clone) == 16);
static_assert(offsetof(FilterOperationVtable, equals) == 24);
static_assert(offsetof(FilterOperationVtable, blend) == 32);
static_assert(offsetof(FilterOperationVtable, transformColor) == 40);
static_assert(offsetof(FilterOperationVtable, inverseTransformColor) == 48);
static_assert(offsetof(FilterOperationVtable, isIdentity) == 56);
static_assert(offsetof(FilterOperationVtable, outsets) == 64);
static_assert(offsetof(FilterOperationVtable, affectsOpacity) == 72);
static_assert(offsetof(FilterOperationVtable, movesPixels) == 80);
static_assert(offsetof(FilterOperationVtable, shouldBeRestrictedBySecurityOrigin) == 88);
static_assert(sizeof(FilterOperationVtableStorage) == 112);
static_assert(offsetof(FilterOperationVtableStorage, offsetToTop) == 0);
static_assert(offsetof(FilterOperationVtableStorage, typeInfo) == 8);
static_assert(offsetof(FilterOperationVtableStorage, dispatch) == 16);
static_assert(sizeof(FilterOperationRef) == 8);
static_assert(alignof(FilterOperationRef) == 8);
static_assert(!std::is_trivially_copyable_v<FilterOperationRef>);
static_assert(sizeof(FilterOperationPtr) == 8);
static_assert(alignof(FilterOperationPtr) == 8);
static_assert(!std::is_trivially_copyable_v<FilterOperationPtr>);
static_assert(sizeof(WebCore::Length) == 8);
static_assert(offsetof(WebCore::Length, m_type) == 4);
static_assert(offsetof(WebCore::Length, m_hasQuirk) == 5);
static_assert(offsetof(WebCore::Length, m_isFloat) == 6);
static_assert(sizeof(WebCore::FilterOperation) == 16);
static_assert(alignof(WebCore::FilterOperation) == 8);
static_assert(offsetof(WebCore::FilterOperation, m_vptr) == 0);
static_assert(offsetof(WebCore::FilterOperation, m_refCount) == 8);
static_assert(offsetof(WebCore::FilterOperation, m_type) == 12);
static_assert(sizeof(WebCore::DefaultFilterOperation) == 24);
static_assert(sizeof(WebCore::BasicColorMatrixFilterOperation) == 24);
static_assert(sizeof(WebCore::BasicComponentTransferFilterOperation) == 24);
static_assert(sizeof(WebCore::InvertLightnessFilterOperation) == 16);
static_assert(sizeof(WebCore::BlurFilterOperation) == 24);
static_assert(sizeof(WebCore::BlurFilterOperation) == sizeof(WebCore::FilterOperation) + sizeof(WebCore::Length));
static_assert(sizeof(WebCore::DropShadowFilterOperation) == 40);
static_assert(sizeof(WebCore::DropShadowFilterOperation) == sizeof(WebCore::FilterOperation) + 8 + 4 + 4 + sizeof(WebCore::Color));
static_assert(sizeof(WebCore::Color) == 8);
static_assert(sizeof(WebCore::IntPoint) == 8);

}  // namespace NKWebKit::FilterAbi

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::TransformAbi {

class TransformOperationRef;

struct TransformOperationVtable {
    void (APS5_VABI* completeDestructor)(WebCore::TransformOperation*);
    void (APS5_VABI* deletingDestructor)(WebCore::TransformOperation*);
    TransformOperationRef (APS5_VABI* clone)(const WebCore::TransformOperation*);
    bool (APS5_VABI* equals)(const WebCore::TransformOperation*, const WebCore::TransformOperation&);
    bool (APS5_VABI* isIdentity)(const WebCore::TransformOperation*);
    bool (APS5_VABI* apply)(const WebCore::TransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
    TransformOperationRef (APS5_VABI* blend)(WebCore::TransformOperation*, const WebCore::TransformOperation*, double, bool);
    bool (APS5_VABI* isAffectedByTransformOrigin)(const WebCore::TransformOperation*);
    bool (APS5_VABI* isRepresentableIn2D)(const WebCore::TransformOperation*);
    void (APS5_VABI* dump)(const WebCore::TransformOperation*, NKWebKit::WtfAbi::TextStream&);
};

struct TransformOperationVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    TransformOperationVtable dispatch;
};

inline const TransformOperationVtable& Dispatch(const WebCore::TransformOperation& operation) {
    return *static_cast<const TransformOperationVtable*>(operation.m_vptr);
}

inline void RefOperation(WebCore::TransformOperation* operation) {
    ++operation->m_refCount;
}

inline void DerefOperation(WebCore::TransformOperation* operation) {
    if (!operation) return;
    if (operation->m_refCount != 1) {
        --operation->m_refCount;
        return;
    }
    Dispatch(*operation).deletingDestructor(operation);
}

class TransformOperationRef {
public:
    explicit TransformOperationRef(WebCore::TransformOperation* operation) : m_operation(operation) {}
    TransformOperationRef(TransformOperationRef&& other) noexcept : m_operation(std::exchange(other.m_operation, nullptr)) {}
    TransformOperationRef(const TransformOperationRef&) = delete;
    TransformOperationRef& operator=(const TransformOperationRef&) = delete;
    TransformOperationRef& operator=(TransformOperationRef&&) = delete;
    ~TransformOperationRef() { DerefOperation(m_operation); }

    WebCore::TransformOperation* Get() const { return m_operation; }
    WebCore::TransformOperation* Leak() { return std::exchange(m_operation, nullptr); }

private:
    WebCore::TransformOperation* m_operation;
};

static_assert(sizeof(TransformOperationVtable) == 80);
static_assert(offsetof(TransformOperationVtable, completeDestructor) == 0);
static_assert(offsetof(TransformOperationVtable, deletingDestructor) == 8);
static_assert(offsetof(TransformOperationVtable, clone) == 16);
static_assert(offsetof(TransformOperationVtable, equals) == 24);
static_assert(offsetof(TransformOperationVtable, isIdentity) == 32);
static_assert(offsetof(TransformOperationVtable, apply) == 40);
static_assert(offsetof(TransformOperationVtable, blend) == 48);
static_assert(offsetof(TransformOperationVtable, isAffectedByTransformOrigin) == 56);
static_assert(offsetof(TransformOperationVtable, isRepresentableIn2D) == 64);
static_assert(offsetof(TransformOperationVtable, dump) == 72);
static_assert(sizeof(TransformOperationVtableStorage) == 96);
static_assert(offsetof(TransformOperationVtableStorage, dispatch) == 16);
static_assert(sizeof(TransformOperationRef) == 8);
static_assert(!std::is_trivially_copyable_v<TransformOperationRef>);

}  // namespace NKWebKit::TransformAbi

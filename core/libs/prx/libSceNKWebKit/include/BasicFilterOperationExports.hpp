#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace WebCore {

template<typename T>
struct BasicFilterOperationRef {
    T* m_ptr{nullptr};

    BasicFilterOperationRef() = default;
    explicit BasicFilterOperationRef(T* ptr) : m_ptr(ptr) {}
    BasicFilterOperationRef(const BasicFilterOperationRef&) = delete;
    BasicFilterOperationRef& operator=(const BasicFilterOperationRef&) = delete;
    BasicFilterOperationRef(BasicFilterOperationRef&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }
    BasicFilterOperationRef& operator=(BasicFilterOperationRef&& other) noexcept {
        if (this != &other) {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }
    ~BasicFilterOperationRef() {
        reset();
    }

    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    T* leakRef() noexcept {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    void reset() {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        if (!ptr)
            return;
        if (ptr->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ptr->m_refCount.store(1, std::memory_order_relaxed);
            using DeletingDestructor = void (APS5_VABI*)(void*);
            auto* vtable = static_cast<DeletingDestructor*>(ptr->m_vptr);
            vtable[1](ptr);
        }
    }
};

static_assert(sizeof(BasicFilterOperationRef<FilterOperation>) == sizeof(void*));
static_assert(alignof(BasicFilterOperationRef<FilterOperation>) == alignof(void*));
static_assert(!std::is_trivially_destructible_v<BasicFilterOperationRef<FilterOperation>>);

} // namespace WebCore

extern "C" {

WebCore::BasicFilterOperationRef<WebCore::BasicColorMatrixFilterOperation> APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperation6createEdNS_15FilterOperation13OperationTypeE(double amount, std::uint32_t type);
void APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperationC1EdNS_15FilterOperation13OperationTypeE(WebCore::BasicColorMatrixFilterOperation* self, double amount, std::uint32_t type);
void APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperationC2EdNS_15FilterOperation13OperationTypeE(WebCore::BasicColorMatrixFilterOperation* self, double amount, std::uint32_t type);
bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(const WebCore::BasicColorMatrixFilterOperation* self, WebCore::SRGBA<float>& color);
WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI _ZN7WebCore31BasicColorMatrixFilterOperation5blendEPKNS_15FilterOperationEdb(WebCore::BasicColorMatrixFilterOperation* self, const WebCore::FilterOperation* from, double progress, bool blendToPassthrough);

WebCore::BasicFilterOperationRef<WebCore::BasicComponentTransferFilterOperation> APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperation6createEdNS_15FilterOperation13OperationTypeE(double amount, std::uint32_t type);
void APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperationC1EdNS_15FilterOperation13OperationTypeE(WebCore::BasicComponentTransferFilterOperation* self, double amount, std::uint32_t type);
void APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperationC2EdNS_15FilterOperation13OperationTypeE(WebCore::BasicComponentTransferFilterOperation* self, double amount, std::uint32_t type);
bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14transformColorERNS_5SRGBAIfEE(const WebCore::BasicComponentTransferFilterOperation* self, WebCore::SRGBA<float>& color);
WebCore::BasicFilterOperationRef<WebCore::FilterOperation> APS5_VABI _ZN7WebCore37BasicComponentTransferFilterOperation5blendEPKNS_15FilterOperationEdb(WebCore::BasicComponentTransferFilterOperation* self, const WebCore::FilterOperation* from, double progress, bool blendToPassthrough);

}

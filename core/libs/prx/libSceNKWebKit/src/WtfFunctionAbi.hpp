#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>

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

class OwnedFunction {
public:
    OwnedFunction() = default;
    explicit OwnedFunction(VoidCallableWrapper* callableWrapper) : m_function{callableWrapper} {}
    OwnedFunction(OwnedFunction&& other) noexcept : m_function{std::exchange(other.m_function.callableWrapper, nullptr)} {}
    OwnedFunction& operator=(OwnedFunction&& other) noexcept {
        OwnedFunction old(std::move(*this));
        m_function.callableWrapper = std::exchange(other.m_function.callableWrapper, nullptr);
        return *this;
    }
    OwnedFunction(const OwnedFunction&) = delete;
    OwnedFunction& operator=(const OwnedFunction&) = delete;
    ~OwnedFunction() {
        if (VoidCallableWrapper* callableWrapper = std::exchange(m_function.callableWrapper, nullptr)) {
            callableWrapper->vtable->deletingDestructor(callableWrapper);
        }
    }

    void Reset() {
        OwnedFunction old(std::move(*this));
    }
    explicit operator bool() const { return m_function.callableWrapper; }
    void operator()() const { m_function.callableWrapper->vtable->call(m_function.callableWrapper); }
    VoidFunction* Pass() { return &m_function; }
    VoidFunction Release() { return VoidFunction{std::exchange(m_function.callableWrapper, nullptr)}; }

private:
    VoidFunction m_function{nullptr};
};

template<typename Callable>
struct HostCallableWrapper : VoidCallableWrapper {
    explicit HostCallableWrapper(Callable function) : VoidCallableWrapper{&kVtable}, callable(std::move(function)) {}

    static void APS5_VABI CompleteDestructor(VoidCallableWrapper* self) {
        static_cast<HostCallableWrapper*>(self)->~HostCallableWrapper();
    }
    static void APS5_VABI DeletingDestructor(VoidCallableWrapper* self) {
        delete static_cast<HostCallableWrapper*>(self);
    }
    static void APS5_VABI Call(VoidCallableWrapper* self) {
        static_cast<HostCallableWrapper*>(self)->callable();
    }

    static constexpr VoidCallableWrapperVtable kVtable{CompleteDestructor, DeletingDestructor, Call};

    Callable callable;
};

template<typename Callable>
OwnedFunction MakeHostFunction(Callable&& callable) {
    return OwnedFunction(new HostCallableWrapper<std::decay_t<Callable>>(std::forward<Callable>(callable)));
}

}  // namespace NKWebKit::WtfAbi

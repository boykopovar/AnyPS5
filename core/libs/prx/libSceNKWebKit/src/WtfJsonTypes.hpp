#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::JsonAbi {

struct Value;
struct ObjectBase;
struct Object;
struct ArrayBase;
struct Array;
struct StringBuilder;

enum class ValueType : int {
    Null = 0,
    Boolean = 1,
    Double = 2,
    Integer = 3,
    String = 4,
    Object = 5,
    Array = 6,
};

void Deref(const Value* value);

template<typename T>
struct Ref {
    T* ptr;

    static Ref Adopt(T* value) {
        return Ref(value, true);
    }

    static Ref Share(T* value) {
        if (value) ++value->refCount;
        return Ref(value, true);
    }

    Ref(const Ref& other) : ptr(other.ptr) {
        if (ptr) ++ptr->refCount;
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Ref(const Ref<U>& other) : ptr(other.ptr) {
        if (ptr) ++ptr->refCount;
    }

    Ref(Ref&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    Ref(Ref<U>&& other) noexcept : ptr(other.Leak()) {}

    Ref& operator=(Ref other) noexcept {
        std::swap(ptr, other.ptr);
        return *this;
    }

    ~Ref() {
        Deref(ptr);
    }

    T* Get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    T* Leak() { return std::exchange(ptr, nullptr); }

private:
    Ref(T* value, bool) : ptr(value) {}
};

template<typename T>
struct RefPtr {
    T* ptr;

    explicit RefPtr(T* value = nullptr) : ptr(value) {
        if (ptr) ++ptr->refCount;
    }

    static RefPtr Adopt(T* value) {
        return RefPtr(value, true);
    }

    static RefPtr Share(T* value) {
        return RefPtr(value);
    }

    RefPtr(const RefPtr& other) : ptr(other.ptr) {
        if (ptr) ++ptr->refCount;
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefPtr(const RefPtr<U>& other) : ptr(other.ptr) {
        if (ptr) ++ptr->refCount;
    }

    RefPtr(RefPtr&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    RefPtr(RefPtr<U>&& other) noexcept : ptr(other.Leak()) {}

    RefPtr& operator=(RefPtr other) noexcept {
        std::swap(ptr, other.ptr);
        return *this;
    }

    ~RefPtr() {
        Deref(ptr);
    }

    T* Get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    explicit operator bool() const { return ptr != nullptr; }
    T* Leak() { return std::exchange(ptr, nullptr); }

private:
    RefPtr(T* value, bool) : ptr(value) {}
};

struct ValueVtable {
    void (APS5_VABI *completeDestructor)(Value*);
    void (APS5_VABI *deletingDestructor)(Value*);
    RefPtr<Object> (APS5_VABI *asObject)(Value*);
    RefPtr<const Object> (APS5_VABI *asObjectConst)(const Value*);
    RefPtr<Array> (APS5_VABI *asArray)(Value*);
    void (APS5_VABI *writeJSON)(const Value*, StringBuilder&);
    std::size_t (APS5_VABI *memoryCost)(const Value*);
};

struct ValueVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    ValueVtable dispatch;
};

union ValueData {
    bool boolean;
    double number;
    NKWebKit::WtfAbi::StringImpl* string;
    std::uint64_t bits;
};

struct Value {
    const ValueVtable* vptr;
    mutable unsigned refCount;
    ValueType valueType;
    ValueData data;
};

inline void Deref(const Value* value) {
    if (!value) return;
    unsigned next = value->refCount - 1;
    if (!next) {
        value->vptr->deletingDestructor(const_cast<Value*>(value));
        return;
    }
    value->refCount = next;
}

template<typename T>
struct Vector {
    T* buffer;
    unsigned capacity;
    unsigned size;
};

template<typename Key, typename Mapped>
struct KeyValuePair {
    Key key;
    Mapped value;
};

template<typename Key, typename Mapped>
struct HashMap {
    KeyValuePair<Key, Mapped>* table;
};

struct ObjectBase : Value {
    HashMap<NKWebKit::WtfAbi::String, Ref<Value>> map;
    Vector<NKWebKit::WtfAbi::String> order;
};

struct Object : ObjectBase {};

struct ArrayBase : Value {
    Vector<Ref<Value>> elements;
};

struct Array : ArrayBase {};

struct StringBuilder {
    mutable NKWebKit::WtfAbi::String string;
    NKWebKit::WtfAbi::String buffer;
    unsigned length{0};
    bool shouldCrashOnOverflow{true};

    StringBuilder() = default;
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
    StringBuilder(StringBuilder&&) noexcept = default;
};

static_assert(sizeof(ValueType) == 4);
static_assert(sizeof(Ref<Value>) == 8);
static_assert(alignof(Ref<Value>) == 8);
static_assert(offsetof(Ref<Value>, ptr) == 0);
static_assert(!std::is_trivially_destructible_v<Ref<Value>>);
static_assert(!std::is_trivially_copyable_v<Ref<Value>>);
static_assert(sizeof(RefPtr<Value>) == 8);
static_assert(alignof(RefPtr<Value>) == 8);
static_assert(offsetof(RefPtr<Value>, ptr) == 0);
static_assert(!std::is_trivially_destructible_v<RefPtr<Value>>);
static_assert(!std::is_trivially_copyable_v<RefPtr<Value>>);
static_assert(sizeof(ValueVtable) == 56);
static_assert(offsetof(ValueVtable, completeDestructor) == 0);
static_assert(offsetof(ValueVtable, deletingDestructor) == 8);
static_assert(offsetof(ValueVtable, asObject) == 16);
static_assert(offsetof(ValueVtable, asObjectConst) == 24);
static_assert(offsetof(ValueVtable, asArray) == 32);
static_assert(offsetof(ValueVtable, writeJSON) == 40);
static_assert(offsetof(ValueVtable, memoryCost) == 48);
static_assert(sizeof(ValueVtableStorage) == 72);
static_assert(offsetof(ValueVtableStorage, offsetToTop) == 0);
static_assert(offsetof(ValueVtableStorage, typeInfo) == 8);
static_assert(offsetof(ValueVtableStorage, dispatch) == 16);
static_assert(sizeof(ValueData) == 8);
static_assert(alignof(ValueData) == 8);
static_assert(sizeof(Value) == 24);
static_assert(alignof(Value) == 8);
static_assert(offsetof(Value, vptr) == 0);
static_assert(offsetof(Value, refCount) == 8);
static_assert(offsetof(Value, valueType) == 12);
static_assert(offsetof(Value, data) == 16);
static_assert(sizeof(Vector<Ref<Value>>) == 16);
static_assert(alignof(Vector<Ref<Value>>) == 8);
static_assert(offsetof(Vector<Ref<Value>>, buffer) == 0);
static_assert(offsetof(Vector<Ref<Value>>, capacity) == 8);
static_assert(offsetof(Vector<Ref<Value>>, size) == 12);
using StringValuePair = KeyValuePair<NKWebKit::WtfAbi::String, Ref<Value>>;
using StringValueMap = HashMap<NKWebKit::WtfAbi::String, Ref<Value>>;
static_assert(sizeof(StringValuePair) == 16);
static_assert(alignof(StringValuePair) == 8);
static_assert(offsetof(StringValuePair, key) == 0);
static_assert(offsetof(StringValuePair, value) == 8);
static_assert(sizeof(StringValueMap) == 8);
static_assert(alignof(StringValueMap) == 8);
static_assert(offsetof(StringValueMap, table) == 0);
static_assert(sizeof(ObjectBase) == 48);
static_assert(alignof(ObjectBase) == 8);
static_assert(sizeof(Object) == 48);
static_assert(alignof(Object) == 8);
static_assert(sizeof(ArrayBase) == 40);
static_assert(alignof(ArrayBase) == 8);
static_assert(sizeof(Array) == 40);
static_assert(alignof(Array) == 8);
static_assert(sizeof(StringBuilder) == 24);
static_assert(alignof(StringBuilder) == 8);
static_assert(offsetof(StringBuilder, string) == 0);
static_assert(offsetof(StringBuilder, buffer) == 8);
static_assert(offsetof(StringBuilder, length) == 16);
static_assert(offsetof(StringBuilder, shouldCrashOnOverflow) == 20);

}  // namespace NKWebKit::JsonAbi

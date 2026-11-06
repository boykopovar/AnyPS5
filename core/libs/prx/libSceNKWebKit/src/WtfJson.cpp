#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::ArrayBase;
using NKWebKit::JsonAbi::Deref;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::StringBuilder;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::JsonAbi::ValueVtableStorage;
using NKWebKit::JsonAbi::Vector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

template<typename T>
T& Checked(T* ptr) {
    if (!ptr) throw std::invalid_argument("null WTF JSON object");
    return *ptr;
}

void APS5_VABI ValueWriteJSON(const Value*, StringBuilder&) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ArrayWriteJSON(const Value*, StringBuilder&) {
    NotImplemented_nid_no_patch(__func__);
}

RefPtr<Object> APS5_VABI ValueAsObject(Value* self) {
    Checked(self);
    return RefPtr<Object>(nullptr);
}

RefPtr<const Object> APS5_VABI ValueAsObjectConst(const Value* self) {
    Checked(self);
    return RefPtr<const Object>(nullptr);
}

RefPtr<Array> APS5_VABI ValueAsArray(Value* self) {
    Checked(self);
    return RefPtr<Array>(nullptr);
}

RefPtr<Array> APS5_VABI ArrayBaseAsArray(Value* self) {
    return RefPtr<Array>::Share(static_cast<Array*>(static_cast<ArrayBase*>(&Checked(self))));
}

extern const ValueVtableStorage ValueVtableInstance;
extern const ValueVtableStorage ArrayVtableInstance;

void APS5_VABI ArrayVtableD1(Value* self) {
    _ZN3WTF8JSONImpl9ArrayBaseD1Ev(static_cast<ArrayBase*>(self));
}

void APS5_VABI ArrayVtableD0(Value* self) {
    _ZN3WTF8JSONImpl9ArrayBaseD0Ev(static_cast<ArrayBase*>(self));
}

std::size_t APS5_VABI ArrayVtableMemoryCost(const Value* self) {
    return _ZNK3WTF8JSONImpl9ArrayBase10memoryCostEv(static_cast<const ArrayBase*>(self));
}

const ValueVtableStorage ValueVtableInstance = {
    0,
    nullptr,
    {
        &_ZN3WTF8JSONImpl5ValueD1Ev,
        &_ZN3WTF8JSONImpl5ValueD0Ev,
        &ValueAsObject,
        &ValueAsObjectConst,
        &ValueAsArray,
        &ValueWriteJSON,
        &_ZNK3WTF8JSONImpl5Value10memoryCostEv,
    },
};

const ValueVtableStorage ArrayVtableInstance = {
    0,
    nullptr,
    {
        &ArrayVtableD1,
        &ArrayVtableD0,
        &ValueAsObject,
        &ValueAsObjectConst,
        &ArrayBaseAsArray,
        &ArrayWriteJSON,
        &ArrayVtableMemoryCost,
    },
};

void InitValue(Value* self, ValueType type) {
    auto& v = Checked(self);
    v.vptr = &ValueVtableInstance.dispatch;
    v.refCount = 1U;
    v.valueType = type;
    v.data.bits = 0ULL;
}

template<typename T>
void GrowVectorCapacity(Vector<T>& vec, unsigned minCapacity) {
    unsigned newCapacity = std::max(minCapacity, std::max(vec.capacity ? vec.capacity * 2U : 4U, 4U));
    auto* newBuffer = static_cast<T*>(_ZN3WTF10fastMallocEm(std::size_t(newCapacity) * sizeof(T)));
    if (vec.buffer) {
        std::memcpy(static_cast<void*>(newBuffer), static_cast<const void*>(vec.buffer), std::size_t(vec.size) * sizeof(T));
        _ZN3WTF8fastFreeEPv(vec.buffer);
    }
    vec.buffer = newBuffer;
    vec.capacity = newCapacity;
}

void AppendValueVector(Vector<Ref<Value>>& vec, Ref<Value>&& item) {
    if (vec.size == vec.capacity) {
        GrowVectorCapacity(vec, vec.size + 1U);
    }
    vec.buffer[vec.size].ptr = item.Leak();
    ++vec.size;
}

void DestroyValueVector(Vector<Ref<Value>>& vec) {
    for (unsigned i = 0U; i < vec.size; ++i) {
        Deref(vec.buffer[i].ptr);
    }
    if (vec.buffer) {
        _ZN3WTF8fastFreeEPv(vec.buffer);
        vec.buffer = nullptr;
    }
    vec.capacity = 0U;
    vec.size = 0U;
}

}  // namespace

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ev(Value* self) {
    InitValue(self, ValueType::Null);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ev(Value* self) {
    _ZN3WTF8JSONImpl5ValueC1Ev(self);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(Value* self, ValueType type) {
    InitValue(self, type);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2ENS1_4TypeE(Value* self, ValueType type) {
    _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(self, type);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Eb(Value* self, bool value) {
    InitValue(self, ValueType::Boolean);
    Checked(self).data.boolean = value;
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Eb(Value* self, bool value) {
    _ZN3WTF8JSONImpl5ValueC1Eb(self, value);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ei(Value* self, int value) {
    InitValue(self, ValueType::Integer);
    Checked(self).data.number = static_cast<double>(value);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ei(Value* self, int value) {
    _ZN3WTF8JSONImpl5ValueC1Ei(self, value);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1Ed(Value* self, double value) {
    InitValue(self, ValueType::Double);
    Checked(self).data.number = value;
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2Ed(Value* self, double value) {
    _ZN3WTF8JSONImpl5ValueC1Ed(self, value);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC1ERKNS_6StringE(Value* self, const String& value) {
    InitValue(self, ValueType::String);
    Checked(self).data.string = value.impl;
    if (value.impl) value.impl->refCount += 2U;
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueC2ERKNS_6StringE(Value* self, const String& value) {
    _ZN3WTF8JSONImpl5ValueC1ERKNS_6StringE(self, value);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueD2Ev(Value* self) {
    auto& v = Checked(self);
    v.vptr = &ValueVtableInstance.dispatch;
    if (v.valueType == ValueType::String && v.data.string) {
        NKWebKit::WtfAbi::Release(v.data.string);
        v.data.string = nullptr;
    }
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueD1Ev(Value* self) {
    _ZN3WTF8JSONImpl5ValueD2Ev(self);
}

void APS5_VABI _ZN3WTF8JSONImpl5ValueD0Ev(Value* self) {
    _ZN3WTF8JSONImpl5ValueD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

Ref<Value> APS5_VABI _ZN3WTF8JSONImpl5Value4nullEv() {
    auto* self = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1Ev(self);
    return Ref<Value>::Adopt(self);
}

Ref<Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEb(bool value) {
    auto* self = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1Eb(self, value);
    return Ref<Value>::Adopt(self);
}

Ref<Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEi(int value) {
    auto* self = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1Ei(self, value);
    return Ref<Value>::Adopt(self);
}

Ref<Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createEd(double value) {
    auto* self = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1Ed(self, value);
    return Ref<Value>::Adopt(self);
}

Ref<Value> APS5_VABI _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(const String& value) {
    auto* self = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1ERKNS_6StringE(self, value);
    return Ref<Value>::Adopt(self);
}

ValueType APS5_VABI _ZNK3WTF8JSONImpl5Value4typeEv(const Value* self) {
    return Checked(self).valueType;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value6isNullEv(const Value* self) {
    return Checked(self).valueType == ValueType::Null;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asDoubleERd(const Value* self, double& output) {
    const auto& v = Checked(self);
    if (v.valueType != ValueType::Double && v.valueType != ValueType::Integer) return false;
    output = v.data.number;
    return true;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asIntegerERi(const Value* self, int& output) {
    const auto& v = Checked(self);
    if (v.valueType != ValueType::Double && v.valueType != ValueType::Integer) return false;
    output = static_cast<int>(v.data.number);
    return true;
}

std::size_t APS5_VABI _ZNK3WTF8JSONImpl5Value10memoryCostEv(const Value* self) {
    const auto& v = Checked(self);
    std::size_t cost = sizeof(self);
    if (v.valueType == ValueType::String && v.data.string) {
        cost += _ZNK3WTF10StringImpl11sizeInBytesEv(v.data.string);
    }
    return cost;
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseC1Ev(ArrayBase* self) {
    auto& arr = Checked(self);
    _ZN3WTF8JSONImpl5ValueC2ENS1_4TypeE(&arr, ValueType::Array);
    arr.vptr = &ArrayVtableInstance.dispatch;
    arr.elements.buffer = nullptr;
    arr.elements.capacity = 0U;
    arr.elements.size = 0U;
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseC2Ev(ArrayBase* self) {
    _ZN3WTF8JSONImpl9ArrayBaseC1Ev(self);
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD1Ev(ArrayBase* self) {
    auto& arr = Checked(self);
    arr.vptr = &ArrayVtableInstance.dispatch;
    DestroyValueVector(arr.elements);
    _ZN3WTF8JSONImpl5ValueD2Ev(&arr);
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD0Ev(ArrayBase* self) {
    _ZN3WTF8JSONImpl9ArrayBaseD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

Ref<Array> APS5_VABI _ZN3WTF8JSONImpl5Array6createEv() {
    auto* self = static_cast<Array*>(_ZN3WTF10fastMallocEm(sizeof(Array)));
    _ZN3WTF8JSONImpl9ArrayBaseC1Ev(self);
    return Ref<Array>::Adopt(self);
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(ArrayBase* self, bool value) {
    AppendValueVector(Checked(self).elements, _ZN3WTF8JSONImpl5Value6createEb(value));
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(ArrayBase* self, int value) {
    AppendValueVector(Checked(self).elements, _ZN3WTF8JSONImpl5Value6createEi(value));
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(ArrayBase* self, double value) {
    AppendValueVector(Checked(self).elements, _ZN3WTF8JSONImpl5Value6createEd(value));
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(ArrayBase* self, const String& value) {
    AppendValueVector(Checked(self).elements, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(value));
}

Ref<Value>* APS5_VABI _ZN3WTF8JSONImpl9ArrayBase5beginEv(ArrayBase* self) {
    return Checked(self).elements.buffer;
}

const Ref<Value>* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase5beginEv(const ArrayBase* self) {
    return Checked(self).elements.buffer;
}

Ref<Value>* APS5_VABI _ZN3WTF8JSONImpl9ArrayBase3endEv(ArrayBase* self) {
    auto& arr = Checked(self);
    return arr.elements.buffer ? arr.elements.buffer + arr.elements.size : nullptr;
}

Ref<Value> APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3getEm(const ArrayBase* self, std::size_t index) {
    const auto& arr = Checked(self);
    if (index >= arr.elements.size) throw std::out_of_range("WTF JSON Array index");
    return Ref<Value>::Share(arr.elements.buffer[index].Get());
}

std::size_t APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase10memoryCostEv(const ArrayBase* self) {
    const auto& arr = Checked(self);
    std::size_t cost = _ZNK3WTF8JSONImpl5Value10memoryCostEv(&arr);
    for (unsigned i = 0U; i < arr.elements.size; ++i) {
        const Value* elem = arr.elements.buffer[i].Get();
        if (elem) cost += elem->vptr->memoryCost(elem);
    }
    return cost;
}

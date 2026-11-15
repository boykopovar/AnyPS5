#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

std::array<unsigned char, 16> ValueHolding(const std::uint64_t* slot) {
    std::array<unsigned char, 16> object{};
    std::memset(object.data(), 0x5A, object.size());
    std::memcpy(object.data(), &slot, sizeof(slot));
    return object;
}

using Object = std::array<unsigned char, 16>;
using Destroy = void (APS5_VABI*)(void*);
using Copy = void (APS5_VABI*)(void*, const void*);
using Assign = void* (APS5_VABI*)(void*, const void*);

static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueD1Ev), Destroy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueD2Ev), Destroy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueC1ERKS0_), Copy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueC2ERKS0_), Copy>);
static_assert(std::is_same_v<decltype(&_ZN10Deprecated11ScriptValueaSERKS0_), Assign>);

Object EmptyValue(unsigned char tail) {
    Object object{};
    object.fill(tail);
    _ZN10Deprecated11ScriptValueC1Ev(object.data());
    return object;
}

bool IsEmptyValue(const Object& object, unsigned char tail) {
    for (std::size_t i = 0; i < 8; ++i)
        if (object[i] != 0) return false;
    for (std::size_t i = 8; i < object.size(); ++i)
        if (object[i] != tail) return false;
    return _ZNK10Deprecated11ScriptValue10hasNoValueEv(object.data()) && _ZNK10Deprecated11ScriptValue7jsValueEv(object.data()) == 0;
}

void TestDestroyEmpty() {
    for (const Destroy destroy : {&_ZN10Deprecated11ScriptValueD1Ev, &_ZN10Deprecated11ScriptValueD2Ev}) {
        Object value = EmptyValue(0x5A);
        destroy(value.data());
        Require(IsEmptyValue(value, 0x5A));
    }
}

void TestCopyEmpty() {
    for (const Copy copy : {&_ZN10Deprecated11ScriptValueC1ERKS0_, &_ZN10Deprecated11ScriptValueC2ERKS0_}) {
        const Object source = EmptyValue(0x5A);
        Object target{};
        target.fill(0xA5);
        copy(target.data(), source.data());
        Require(IsEmptyValue(target, 0xA5));
        Require(IsEmptyValue(source, 0x5A));
    }
}

void TestAssignEmpty() {
    Object target = EmptyValue(0x5A);
    const Object source = EmptyValue(0xA5);
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), source.data()) == target.data());
    Require(IsEmptyValue(target, 0x5A));
    Require(IsEmptyValue(source, 0xA5));
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(target.data(), target.data()) == target.data());
    Require(IsEmptyValue(target, 0x5A));
}

struct ScriptObject {
    unsigned char value[8];
    void* globalObject;
};

static_assert(sizeof(ScriptObject) == 16);

void TestScriptObjectLifetime() {
    int firstGlobal = 0;
    int secondGlobal = 0;
    ScriptObject first{};
    std::memset(first.value, 0x5A, sizeof(first.value));
    _ZN10Deprecated11ScriptValueC2Ev(&first);
    first.globalObject = &firstGlobal;
    ScriptObject copy{};
    std::memset(copy.value, 0x5A, sizeof(copy.value));
    _ZN10Deprecated11ScriptValueC2ERKS0_(&copy, &first);
    copy.globalObject = first.globalObject;
    ScriptObject second{};
    _ZN10Deprecated11ScriptValueC2Ev(&second);
    second.globalObject = &secondGlobal;
    Require(_ZN10Deprecated11ScriptValueaSERKS0_(&second, &copy) == &second);
    second.globalObject = copy.globalObject;
    for (ScriptObject* object : {&first, &copy, &second}) {
        Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(object));
        Require(object->globalObject == &firstGlobal);
        _ZN10Deprecated11ScriptValueD2Ev(object);
        Require(object->globalObject == &firstGlobal);
        for (const unsigned char byte : object->value) Require(byte == 0);
    }
}

} // namespace

int main() {
    std::array<unsigned char, 16> empty{};
    std::memset(empty.data(), 0x5A, empty.size());
    _ZN10Deprecated11ScriptValueC1Ev(empty.data());
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(empty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(empty.data()) == 0);

    for (const std::uint64_t encoded : {std::uint64_t{0x0002000000000007}, std::uint64_t{0xFFFE000000000001}, std::uint64_t{1}}) {
        std::uint64_t slot = encoded;
        const auto object = ValueHolding(&slot);
        Require(!_ZNK10Deprecated11ScriptValue10hasNoValueEv(object.data()));
        Require(_ZNK10Deprecated11ScriptValue7jsValueEv(object.data()) == encoded);
        Require(slot == encoded);
        for (std::size_t i = sizeof(&slot); i < object.size(); ++i) Require(object[i] == 0x5A);
    }

    std::uint64_t emptySlot = 0;
    const auto holdingEmpty = ValueHolding(&emptySlot);
    Require(_ZNK10Deprecated11ScriptValue10hasNoValueEv(holdingEmpty.data()));
    Require(_ZNK10Deprecated11ScriptValue7jsValueEv(holdingEmpty.data()) == 0);

    TestDestroyEmpty();
    TestCopyEmpty();
    TestAssignEmpty();
    TestScriptObjectLifetime();
}

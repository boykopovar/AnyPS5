#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };

static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}

static std::string Serialize(Value& value) {
    String output{};
    _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Serialization must succeed");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}

static bool ReferArrayThrows(Value& value) {
    try {
        _ZN3sce4Json5Value10referArrayEv(&value);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ReferObjectThrows(Value& value) {
    try {
        _ZN3sce4Json5Value11referObjectEv(&value);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 0, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "Initialize the Json library");

    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    Array* array = _ZN3sce4Json5Value10referArrayEv(&value);
    Require(array != nullptr && _ZNK3sce4Json5Value7getTypeEv(&value) == TypeArray && value.array == array,
            "referArray must turn a null value into an array it holds");
    Require(_ZNK3sce4Json5Array4sizeEv(array) == 0 && Serialize(value) == "[]", "The new array must be empty");
    Value element{};
    _ZN3sce4Json5ValueC1El(&element, 7);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(array, &element);
    _ZN3sce4Json5ValueD1Ev(&element);
    Require(Serialize(value) == "[7]", "Elements added through the returned array must belong to the value");
    Require(_ZN3sce4Json5Value10referArrayEv(&value) == array, "referArray on an array must return that array");
    Require(ReferObjectThrows(value), "referObject on an array must throw");
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeArray && Serialize(value) == "[7]", "A failed referObject must keep the array");
    _ZN3sce4Json5ValueD1Ev(&value);

    _ZN3sce4Json5ValueC1Ev(&value);
    Object* object = _ZN3sce4Json5Value11referObjectEv(&value);
    Require(object != nullptr && _ZNK3sce4Json5Value7getTypeEv(&value) == TypeObject && value.object == object,
            "referObject must turn a null value into an object it holds");
    Require(_ZNK3sce4Json6Object4sizeEv(object) == 0 && Serialize(value) == "{}", "The new object must be empty");
    Require(_ZN3sce4Json5Value11referObjectEv(&value) == object, "referObject on an object must return that object");
    Require(ReferArrayThrows(value), "referArray on an object must throw");
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeObject && Serialize(value) == "{}", "A failed referArray must keep the object");
    _ZN3sce4Json5ValueD1Ev(&value);

    _ZN3sce4Json5ValueC1El(&value, 5);
    Require(ReferArrayThrows(value) && ReferObjectThrows(value), "referArray and referObject on an integer must throw");
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger && Serialize(value) == "5", "A failed refer must keep the integer");
    _ZN3sce4Json5ValueD1Ev(&value);
    return 0;
}

#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

using namespace SceJson2;

struct Parameter {
    void* allocator;
    void* userData;
    std::size_t bufferSize;
    std::int32_t mode;
    std::uint32_t reserved[3];
};
static_assert(sizeof(Parameter) == 40);

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::string Serialize(Value& value, bool toString = false) {
    String output;
    _ZN3sce4Json6StringC1Ev(&output);
    if (toString) _ZNK3sce4Json5Value8toStringERNS0_6StringE(&value, &output);
    else Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0,
                 "Float serialization must succeed");
    const std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}

static void CheckMode(int mode) {
    JsonTest::Allocator allocator;
    int userData = 0;
    Parameter parameter{&allocator, &userData, 16384, mode, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "Initialize a valid float configuration");
    parameter.mode = (mode + 1) % 3;
    const std::array<double, 5> numbers{std::numeric_limits<double>::infinity(),
                                        -std::numeric_limits<double>::infinity(),
                                        std::numeric_limits<double>::quiet_NaN(),
                                        std::bit_cast<double>(std::uint64_t{0x7ff80000cafef00d}),
                                        std::bit_cast<double>(std::uint64_t{0xfff80000a5a55a5a})};
    const std::array<const char*, 5> tokens{"inf", "-inf", "nan", "nan", "nan"};
    for (unsigned i = 0; i < numbers.size(); ++i) {
        Value value;
        _ZN3sce4Json5ValueC1Ed(&value, numbers[i]);
        const int type = mode == 0 ? 5 : mode == 1 ? 0 : 4;
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == type,
                "Non-finite construction must honor the initialization mode");
        const std::string expected = mode == 0 ? '"' + std::string(tokens[i]) + '"'
                                             : mode == 1 ? "null" : tokens[i];
        Require(Serialize(value) == expected, "Non-finite serialization must honor the copied mode");
        Require(Serialize(value, true) == (mode == 0 ? tokens[i] : expected),
                "toString must return unquoted string values and serialize other types");
        if (mode == 2)
            Require(std::bit_cast<std::uint64_t>(*_ZNK3sce4Json5Value7getRealEv(&value)) ==
                        std::bit_cast<std::uint64_t>(numbers[i]),
                    "Mode 2 construction and formatting must preserve the real payload");
        _ZN3sce4Json5Value3setEd(&value, 0.125);
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 4 &&
                    *_ZNK3sce4Json5Value7getRealEv(&value) == 0.125 && Serialize(value) == "0.125",
                "Finite assignment and formatting must remain unchanged");
        _ZN3sce4Json5Value3setEd(&value, numbers[i]);
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == type && Serialize(value) == expected,
                "Non-finite assignment must use the same conversion as construction");
        if (mode == 2)
            Require(std::bit_cast<std::uint64_t>(*_ZNK3sce4Json5Value7getRealEv(&value)) ==
                        std::bit_cast<std::uint64_t>(numbers[i]),
                    "Mode 2 assignment must preserve the real payload");
        Array array;
        _ZN3sce4Json5ArrayC1Ev(&array);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value);
        Value nested;
        _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&nested, &array);
        Require(Serialize(nested) == '[' + expected + ']', "Nested values must retain their converted type");
        _ZN3sce4Json5ValueD1Ev(&nested);
        _ZN3sce4Json5ArrayD1Ev(&array);
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    Value zero;
    _ZN3sce4Json5ValueC1Ed(&zero, -0.0);
    Require(std::signbit(*_ZNK3sce4Json5Value7getRealEv(&zero)), "All modes must preserve finite signed zero");
    _ZN3sce4Json5ValueD1Ev(&zero);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0,
            "Termination must reset initializer state");
    _ZN3sce4Json11InitializerD1Ev(&initializer);
}

static void CheckLifecycle() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 0, {}};
    std::array<std::uint8_t, 16> storage;
    storage.fill(0xa5);
    _ZN3sce4Json11InitializerC1Ev(storage.data());
    Require(storage[0] == 0, "The initializer constructor must clear its active byte");
    for (unsigned i = 1; i < storage.size(); ++i)
        Require(storage[i] == 0xa5, "The initializer constructor must preserve trailing storage");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(storage.data())) == 0x80848110,
            "Termination before initialization must report not initialized");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(storage.data(), nullptr)) == 0x80848120,
            "Null InitParameter2 must report invalid argument");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(storage.data(), nullptr)) == 0x80848120,
            "Null legacy parameters must report invalid argument");
    Parameter noAllocator{};
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(storage.data(), reinterpret_cast<const InitParameter2*>(&noAllocator))) == 0x80848120,
            "InitParameter2 must reject a null allocator");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(storage.data(), &noAllocator)) == 0x80848102,
            "Legacy initialization must report its observed null-allocator error");
    for (int mode : {-1, 3, std::numeric_limits<int>::max()}) {
        parameter.mode = mode;
        Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(storage.data(), reinterpret_cast<const InitParameter2*>(&parameter))) == 0x80848120,
                "Invalid float modes must be rejected");
        Require(storage[0] == 0, "Rejected initialization must preserve the inactive byte");
    }
    parameter.mode = 0;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(storage.data(), reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "A valid initialization must work after invalid modes");
    std::uint8_t other = 0;
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&other, reinterpret_cast<const InitParameter2*>(std::uintptr_t{1}))) == 0x80848111,
            "Already initialized must precede parameter access");
    Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(&other)) == 0x80848110,
            "An inactive initializer must not terminate active configuration");
    Value value;
    _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
    Require(Serialize(value) == "\"inf\"", "Rejected lifecycle calls must preserve the active mode");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json11InitializerD1Ev(storage.data());
    Require(storage[0] == 0, "The destructor must terminate an active initializer");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&other, &parameter) == 0,
            "Legacy initialization must work after destructor cleanup");
    _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == 4 &&
                std::isinf(*_ZNK3sce4Json5Value7getRealEv(&value)),
            "Legacy initialization must not apply InitParameter2 conversion rules");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json11InitializerD1Ev(&other);
}

int main() {
    for (int mode = 0; mode < 3; ++mode) CheckMode(mode);
    CheckLifecycle();
    return 0;
}

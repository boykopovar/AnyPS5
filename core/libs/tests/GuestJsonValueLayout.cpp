#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace SceJson2;

static_assert(sizeof(Value) == 32 && offsetof(Value, payload) == 16 && offsetof(Value, type) == 28);
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static void Prefix(const Value& value, int type) {
    Require(!value.parent && !value.auxiliary && value.type == type, "Value must expose its native parent, auxiliary and inline type fields");
    Require(value.reserved == 0xa5a5a5a5U, "Value construction must preserve the reserved word");
}
int main() {
    alignas(8) std::array<Value, 3> storage;
    std::memset(storage.data(), 0xa5, sizeof(storage));
    auto& value = storage[1];
    _ZN3sce4Json5ValueC1Ev(&value);
    Prefix(value, 0);
    Require(value.payload == 0xa5a5a5a5a5a5a5a5ULL, "Null construction must preserve inactive payload bytes");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1Eb(&value, true);
    Prefix(value, 1);
    Require(value.payload == 0xa5a5a5a5a5a5a501ULL, "Boolean construction must write only its one-byte payload");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1El(&value, -73);
    Prefix(value, 2);
    Require(std::bit_cast<std::int64_t>(value.payload) == -73 &&
                _ZNK3sce4Json5Value10getIntegerEv(&value) == reinterpret_cast<const std::int64_t*>(&value.payload),
            "Signed integer storage must be inline and returned by reference");
    Require(_ZNK3sce4Json5Value11getUIntegerEv(&value) == &value.payload,
            "Both integer getters must refer to the same inline bits");
    Require(*_ZNK3sce4Json5Value7getRealEv(&value) == 0.0 && value.type == 2 &&
                std::bit_cast<std::int64_t>(value.payload) == -73,
            "An unmatched real getter must leave integer storage unchanged and return the default real");
    Value copy;
    std::memset(&copy, 0xa5, sizeof(copy));
    _ZN3sce4Json5ValueC1ERKS1_(&copy, &value);
    Prefix(copy, 2);
    Require(copy.payload == value.payload && _ZNK3sce4Json5Value10getIntegerEv(&copy) != _ZNK3sce4Json5Value10getIntegerEv(&value),
            "A scalar copy must own independent inline storage");
    _ZN3sce4Json5Value3setEl(&copy, 19);
    Require(std::bit_cast<std::int64_t>(value.payload) == -73 && copy.payload == 19, "Assignment must not alias a scalar source");
    _ZN3sce4Json5Value5clearEv(&copy);
    Require(copy.type == 0 && copy.payload == 19 && copy.reserved == 0xa5a5a5a5U,
            "Clear must reset type while preserving inactive scalar bytes");
    _ZN3sce4Json5ValueD1Ev(&copy);
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1Em(&value, 0xfedcba9876543210ULL);
    Prefix(value, 3);
    Require(value.payload == 0xfedcba9876543210ULL && *_ZNK3sce4Json5Value11getUIntegerEv(&value) == value.payload,
            "Unsigned construction must preserve all 64 payload bits");
    _ZN3sce4Json5ValueD1Ev(&value);
    for (std::uint64_t bits : {0x3fc0000000000000ULL, 0x8000000000000000ULL, 0x7ff80000cafef00dULL}) {
        _ZN3sce4Json5ValueC1Ed(&value, std::bit_cast<double>(bits));
        Prefix(value, 4);
        Require(value.payload == bits && _ZNK3sce4Json5Value7getRealEv(&value) == reinterpret_cast<const double*>(&value.payload),
                "Real construction must preserve inline payload bits and reference identity");
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == value.type, "The getter must read the inline type");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (unsigned index : {0U, 2U}) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&storage[index]);
        for (std::size_t offset = 0; offset < sizeof(Value); ++offset)
            Require(bytes[offset] == 0xa5, "Value operations must not overwrite neighboring objects");
    }
    return 0;
}

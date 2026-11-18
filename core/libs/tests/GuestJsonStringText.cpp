#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };

static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}

int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 0, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "Initialize the Json library");

    const char* const unmapped = reinterpret_cast<const char*>(0x10);

    String str{};
    _ZN3sce4Json6StringC1EPKc(&str, "hello");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&str), "hello") == 0, "Initial string must match");
    Require(_ZNK3sce4Json6String6lengthEv(&str) == 5, "Length must be 5");
    Require(_ZNK3sce4Json6StringeqEPKc(&str, "hello"), "Equality must hold");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, "other"), "Inequality must hold");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, nullptr), "Null string must not be equal");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, unmapped), "Unmapped string must not be equal");

    _ZN3sce4Json6String6appendEPKc(&str, " world");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&str), "hello world") == 0, "Append text must match");

    _ZN3sce4Json6String6appendEPKcm(&str, "!", 1);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&str), "hello world!") == 0, "Append bounded text must match");

    _ZN3sce4Json6StringpLEPKc(&str, " test");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&str), "hello world! test") == 0, "Operator+= must match");

    Require(_ZNK3sce4Json6String7compareEPKc(&str, "hello world! test") == 0, "Compare identical must return 0");

    Require(_ZNK3sce4Json6String4findEPKcm(&str, "world", 0) == 6, "Find must locate substring");
    Require(_ZNK3sce4Json6String4findEPKcm(&str, unmapped, 0) == _ZN3sce4Json6String4nposE, "Find unmapped must return npos");
    Require(_ZNK3sce4Json6String4findEPKcmm(&str, unmapped, 0, 5) == _ZN3sce4Json6String4nposE, "Find unmapped bounded must return npos");

    Require(_ZNK3sce4Json6String5rfindEPKcm(&str, "world", _ZN3sce4Json6String4nposE) == 6, "Rfind must locate substring");
    Require(_ZNK3sce4Json6String5rfindEPKcm(&str, unmapped, _ZN3sce4Json6String4nposE) == _ZN3sce4Json6String4nposE, "Rfind unmapped must return npos");
    Require(_ZNK3sce4Json6String5rfindEPKcmm(&str, unmapped, _ZN3sce4Json6String4nposE, 5) == _ZN3sce4Json6String4nposE, "Rfind unmapped bounded must return npos");

    bool threw = false;
    try {
        _ZN3sce4Json6String6appendEPKc(&str, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Append unmapped must throw");

    threw = false;
    try {
        _ZN3sce4Json6String6appendEPKcm(&str, unmapped, 5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Append bounded unmapped must throw");

    threw = false;
    try {
        _ZN3sce4Json6String7compareEPKc(&str, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Compare unmapped must throw");

    _ZN3sce4Json6StringD1Ev(&str);

    String emptyFromUnmapped{};
    _ZN3sce4Json6StringC1EPKc(&emptyFromUnmapped, unmapped);
    Require(_ZNK3sce4Json6String6lengthEv(&emptyFromUnmapped) == 0, "Unmapped constructor must produce empty string");
    _ZN3sce4Json6StringD1Ev(&emptyFromUnmapped);

    String emptyFromNull{};
    _ZN3sce4Json6StringC1EPKc(&emptyFromNull, nullptr);
    Require(_ZNK3sce4Json6String6lengthEv(&emptyFromNull) == 0, "Null constructor must produce empty string");
    _ZN3sce4Json6StringD1Ev(&emptyFromNull);

    return 0;
}

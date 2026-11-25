#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
    constexpr auto none = std::numeric_limits<std::size_t>::max();

    String str{};
    _ZN3sce4Json6StringC1EPKc(&str, "hello");
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "hello", "Construct string from text");
    Require(_ZNK3sce4Json6String4sizeEv(&str) == 5, "Size must be 5");
    _ZN3sce4Json6StringD1Ev(&str);

    _ZN3sce4Json6StringC1EPKc(&str, nullptr);
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "", "Construct string from null");
    Require(_ZNK3sce4Json6String4sizeEv(&str) == 0, "Null string size must be 0");
    _ZN3sce4Json6StringD1Ev(&str);

    _ZN3sce4Json6StringC1EPKc(&str, unmapped);
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "", "Construct string from unmapped");
    Require(_ZNK3sce4Json6String4sizeEv(&str) == 0, "Unmapped string size must be 0");
    _ZN3sce4Json6StringD1Ev(&str);

    _ZN3sce4Json6StringC1EPKc(&str, "test");
    Require(_ZNK3sce4Json6StringeqEPKc(&str, "test"), "Equal text must match");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, "other"), "Different text must not match");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, nullptr), "Null text must not match");
    Require(!_ZNK3sce4Json6StringeqEPKc(&str, unmapped), "Unmapped text must not match");

    _ZN3sce4Json6String6appendEPKc(&str, "ing");
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing", "Append text must extend string");

    bool threw = false;
    try {
        _ZN3sce4Json6String6appendEPKc(&str, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing", "Append null must throw and keep string");

    threw = false;
    try {
        _ZN3sce4Json6String6appendEPKc(&str, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing", "Append unmapped must throw and keep string");

    threw = false;
    try {
        _ZN3sce4Json6StringpLEPKc(&str, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing", "Operator+= unmapped must throw and keep string");

    _ZN3sce4Json6String6appendEPKcm(&str, " 123", 2);
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing 1", "Counted append must add prefix");

    _ZN3sce4Json6String6appendEPKcm(&str, nullptr, 0);
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing 1", "Counted append null with zero length must succeed");

    _ZN3sce4Json6String6appendEPKcm(&str, unmapped, 0);
    Require(std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing 1", "Counted append unmapped with zero length must succeed");

    threw = false;
    try {
        _ZN3sce4Json6String6appendEPKcm(&str, nullptr, 3);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Counted append null with non-zero length must throw");

    threw = false;
    try {
        _ZN3sce4Json6String6appendEPKcm(&str, unmapped, 3);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && std::string(_ZNK3sce4Json6String5c_strEv(&str)) == "testing 1", "Counted append unmapped with non-zero length must throw and keep string");

    Require(_ZNK3sce4Json6String7compareEPKc(&str, "testing 1") == 0, "Compare equal text must return 0");

    threw = false;
    try {
        _ZNK3sce4Json6String7compareEPKc(&str, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Compare null text must throw");

    threw = false;
    try {
        _ZNK3sce4Json6String7compareEPKc(&str, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Compare unmapped text must throw");

    Require(_ZNK3sce4Json6String4findEPKcm(&str, "ing", 0) == 4, "Find must locate substring");
    Require(_ZNK3sce4Json6String4findEPKcm(&str, unmapped, 0) == none, "Find unmapped needle must return none");
    Require(_ZNK3sce4Json6String5rfindEPKcm(&str, unmapped, 0) == none, "Rfind unmapped needle must return none");
    Require(_ZNK3sce4Json6String4findEPKcmm(&str, unmapped, 0, 4) == none, "Counted find unmapped needle must return none");
    Require(_ZNK3sce4Json6String5rfindEPKcmm(&str, unmapped, 0, 4) == none, "Counted rfind unmapped needle must return none");
    Require(_ZNK3sce4Json6String4findEPKcmm(&str, unmapped, 0, 0) == 0, "Counted find zero length must return position");
    Require(_ZNK3sce4Json6String5rfindEPKcmm(&str, unmapped, 0, 0) == 0, "Counted rfind zero length must return position");

    _ZN3sce4Json6StringD1Ev(&str);
    return 0;
}

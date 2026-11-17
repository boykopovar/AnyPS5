#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include "Testing.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };

static std::string Serialize(Value& value) {
    String output{};
    _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Serialization must succeed");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}

int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 0, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0,
            "Initialize the Json library");

    const char* const unmapped = reinterpret_cast<const char*>(0x10);
    Value value{};
    _ZN3sce4Json5ValueC1EPKc(&value, "text");
    Require(Serialize(value) == "\"text\"", "A text value must hold its text");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1EPKc(&value, nullptr);
    Require(Serialize(value) == "\"\"", "A null text must make an empty string");
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1EPKc(&value, unmapped);
    Require(Serialize(value) == "\"\"", "An unreadable text must make an empty string like a null one");
    _ZN3sce4Json5ValueD1Ev(&value);
    const std::string longText(100000, 'x');
    _ZN3sce4Json5ValueC1EPKc(&value, longText.c_str());
    Require(Serialize(value) == "\"" + longText + "\"", "A long text must be kept whole");

    _ZN3sce4Json5Value3setEPKc(&value, "updated");
    Require(Serialize(value) == "\"updated\"", "set must replace the text");
    bool threw = false;
    try {
        _ZN3sce4Json5Value3setEPKc(&value, unmapped);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && Serialize(value) == "\"updated\"", "set with unreadable text must throw and keep the value");
    _ZN3sce4Json5ValueD1Ev(&value);
    return 0;
}

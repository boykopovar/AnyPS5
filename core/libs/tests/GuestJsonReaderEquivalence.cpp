#include "prx/libSceJson2/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

using namespace SceJson2;

struct Reader { std::string_view text; std::size_t position = 0; };
static int APS5_VABI Read(char& byte, void* context) {
    auto& reader = *static_cast<Reader*>(context);
    if (reader.position == reader.text.size()) return -1;
    byte = reader.text[reader.position++];
    return 0;
}

static void Check(Value& value, unsigned index) {
    if (index == 0) {
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeObject);
        Require(_ZNK3sce4Json5Value5countEv(&value) == 3);
        const auto* array = _ZNK3sce4Json5ValueixEPKc(&value, "values");
        Require(_ZNK3sce4Json5Value7getTypeEv(array) == TypeArray && _ZNK3sce4Json5Value5countEv(array) == 4);
        const auto* number = _ZNK3sce4Json5ValueixEm(array, 0);
        Require(_ZNK3sce4Json5Value7getTypeEv(number) == TypeInteger && *_ZNK3sce4Json5Value10getIntegerEv(number) == -7);
        Require(*_ZNK3sce4Json5Value7getRealEv(_ZNK3sce4Json5ValueixEm(array, 1)) == 1.5);
        Require(*_ZNK3sce4Json5Value10getBooleanEv(_ZNK3sce4Json5ValueixEm(array, 2)));
        Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5ValueixEm(array, 3)) == TypeNull);
        const auto* string = _ZNK3sce4Json5Value9getStringEv(_ZNK3sce4Json5ValueixEPKc(&value, "text"));
        const std::string expected = "\b\f\n\r\t\\\"/\xc3\xa9\xf0\x9f\x98\x80";
        Require(std::string(_ZNK3sce4Json6String5c_strEv(string), _ZNK3sce4Json6String6lengthEv(string)) == expected);
        const auto* empty = _ZNK3sce4Json5ValueixEPKc(&value, "empty");
        Require(_ZNK3sce4Json5Value7getTypeEv(empty) == TypeObject && _ZNK3sce4Json5Value5countEv(empty) == 0);
    } else if (index == 1) {
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeArray && _ZNK3sce4Json5Value5countEv(&value) == 0);
    } else {
        const Value* current = &value;
        for (int depth = 0; depth < 32; ++depth) {
            Require(_ZNK3sce4Json5Value7getTypeEv(current) == TypeArray && _ZNK3sce4Json5Value5countEv(current) == 1);
            current = _ZNK3sce4Json5ValueixEm(current, 0);
        }
        Require(_ZNK3sce4Json5Value7getTypeEv(current) == TypeUInteger && *_ZNK3sce4Json5Value11getUIntegerEv(current) == 42);
    }
}

int main() {
    const std::array<std::string, 3> documents{
        R"json( {"values":[-7,1.5,true,null],"text":"\b\f\n\r\t\\\"\/\u00e9\ud83d\ude00","empty":{}} )json",
        " \n\t[]\r ", std::string(32, '[') + "42" + std::string(32, ']')};
    for (unsigned index = 0; index < documents.size(); ++index) {
        for (const bool callback : {false, true}) {
            Value value{};
            _ZN3sce4Json5ValueC1Ev(&value);
            Reader reader{documents[index]};
            const int status = callback ? _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&value, Read, &reader)
                : _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, documents[index].data(), documents[index].size());
            Require(status == 0);
            Check(value, index);
            _ZN3sce4Json5ValueD1Ev(&value);
        }
    }
}

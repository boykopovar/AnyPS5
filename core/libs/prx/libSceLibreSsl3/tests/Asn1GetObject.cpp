#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ASN1_get_object check failed at line %u\n", location.line());
    std::abort();
}

template <typename Callable>
void RequireInvalidArgument(Callable call) {
    bool thrown = false;
    try {
        call();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

struct Parsed {
    int result = 0;
    const unsigned char* pointer = nullptr;
    std::int64_t length = -1;
    int tag = -1;
    int objectClass = -1;
};

Parsed Parse(const unsigned char* encoded, std::int64_t maximum) {
    Parsed parsed;
    parsed.pointer = encoded;
    parsed.result = ASN1_get_object_nid_postfix(&parsed.pointer, &parsed.length, &parsed.tag, &parsed.objectClass, maximum);
    return parsed;
}

void ShortSequence() {
    const unsigned char encoded[] = {0x30, 0x03, 0x02, 0x01, 0x2a};
    const Parsed parsed = Parse(encoded, static_cast<std::int64_t>(sizeof(encoded)));
    Require(parsed.result == 0x20);
    Require(parsed.tag == 0x10 && parsed.objectClass == 0x00);
    Require(parsed.length == 3);
    Require(parsed.pointer == encoded + 2);
    Require(parsed.pointer[0] == 0x02 && parsed.pointer[1] == 0x01 && parsed.pointer[2] == 0x2a);
}

void LongFormLength() {
    std::array<unsigned char, 131> encoded{};
    encoded[0] = 0x30;
    encoded[1] = 0x81;
    encoded[2] = 0x80;
    for (std::size_t index = 3; index < encoded.size(); ++index)
        encoded[index] = static_cast<unsigned char>(index);
    const Parsed parsed = Parse(encoded.data(), static_cast<std::int64_t>(encoded.size()));
    Require(parsed.result == 0x20);
    Require(parsed.tag == 0x10 && parsed.objectClass == 0x00);
    Require(parsed.length == 128);
    Require(parsed.pointer == encoded.data() + 3);
    Require(parsed.pointer[0] == 3 && parsed.pointer[127] == static_cast<unsigned char>(130));
}

void IndefiniteLength() {
    const unsigned char encoded[] = {0x30, 0x80, 0x00, 0x00};
    const Parsed parsed = Parse(encoded, static_cast<std::int64_t>(sizeof(encoded)));
    Require(parsed.result == 0x21);
    Require(parsed.tag == 0x10 && parsed.objectClass == 0x00);
    Require(parsed.length == 0);
    Require(parsed.pointer == encoded + 2);
}

void PrimitiveInteger() {
    const unsigned char encoded[] = {0x02, 0x01, 0x2a};
    const Parsed parsed = Parse(encoded, static_cast<std::int64_t>(sizeof(encoded)));
    Require(parsed.result == 0x00);
    Require(parsed.tag == 0x02 && parsed.objectClass == 0x00);
    Require(parsed.length == 1);
    Require(parsed.pointer == encoded + 2 && parsed.pointer[0] == 0x2a);
}

void ExplicitClass() {
    const unsigned char encoded[] = {0xa3, 0x01, 0x00};
    const Parsed parsed = Parse(encoded, static_cast<std::int64_t>(sizeof(encoded)));
    Require(parsed.result == 0x20);
    Require(parsed.tag == 0x03 && parsed.objectClass == 0x80);
    Require(parsed.length == 1);
    Require(parsed.pointer == encoded + 2);
}

void IncompleteHeader() {
    const unsigned char truncated[] = {0x30};
    const Parsed first = Parse(truncated, static_cast<std::int64_t>(sizeof(truncated)));
    Require(first.result == 0x80);
    Require(first.pointer == truncated && first.length == -1 && first.tag == -1 && first.objectClass == -1);

    const unsigned char shortLength[] = {0x30, 0x81};
    const Parsed second = Parse(shortLength, static_cast<std::int64_t>(sizeof(shortLength)));
    Require(second.result == 0x80);
    Require(second.pointer == shortLength && second.length == -1);
    Require(second.tag == 0x10 && second.objectClass == 0x00);
}

void Tags() {
    const unsigned char highTag[] = {0x1f, 0x81, 0x00, 0x01, 0x2a};
    const Parsed parsed = Parse(highTag, static_cast<std::int64_t>(sizeof(highTag)));
    Require(parsed.result == 0x00);
    Require(parsed.tag == 128 && parsed.objectClass == 0x00);
    Require(parsed.length == 1);
    Require(parsed.pointer == highTag + 4 && parsed.pointer[0] == 0x2a);

    const unsigned char overlong[] = {0x1f, 0x81, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    const Parsed rejected = Parse(overlong, static_cast<std::int64_t>(sizeof(overlong)));
    Require(rejected.result == 0x80);
    Require(rejected.pointer == overlong && rejected.length == -1 && rejected.tag == -1 && rejected.objectClass == -1);
}

void TooLongLength() {
    const unsigned char constructed[] = {0x30, 0x03, 0x02};
    const Parsed first = Parse(constructed, static_cast<std::int64_t>(sizeof(constructed)));
    Require(first.result == 0xa0);
    Require(first.tag == 0x10 && first.objectClass == 0x00);
    Require(first.length == 3);
    Require(first.pointer == constructed + 2);

    const unsigned char primitive[] = {0x02, 0x02, 0x01};
    const Parsed second = Parse(primitive, static_cast<std::int64_t>(sizeof(primitive)));
    Require(second.result == 0x80);
    Require(second.tag == 0x02 && second.objectClass == 0x00);
    Require(second.length == 2);
    Require(second.pointer == primitive + 2);
}

void ZeroMaximum() {
    const unsigned char encoded[] = {0x30, 0x00};
    const Parsed parsed = Parse(encoded, 0);
    Require(parsed.result == 0x80);
    Require(parsed.pointer == encoded && parsed.length == -1 && parsed.tag == -1 && parsed.objectClass == -1);
}

void IndefinitePrimitive() {
    const unsigned char encoded[] = {0x02, 0x80};
    const Parsed parsed = Parse(encoded, static_cast<std::int64_t>(sizeof(encoded)));
    Require(parsed.result == 0x80);
    Require(parsed.tag == 0x02 && parsed.objectClass == 0x00);
    Require(parsed.length == 0);
    Require(parsed.pointer == encoded);
}

void NullArguments() {
    const unsigned char encoded[] = {0x30, 0x00};
    const unsigned char* pointer = encoded;
    std::int64_t length = 0;
    int tag = 0;
    int objectClass = 0;
    RequireInvalidArgument([&] { ASN1_get_object_nid_postfix(nullptr, &length, &tag, &objectClass, 2); });
    RequireInvalidArgument([&] { ASN1_get_object_nid_postfix(&pointer, nullptr, &tag, &objectClass, 2); });
    RequireInvalidArgument([&] { ASN1_get_object_nid_postfix(&pointer, &length, nullptr, &objectClass, 2); });
    RequireInvalidArgument([&] { ASN1_get_object_nid_postfix(&pointer, &length, &tag, nullptr, 2); });
    const unsigned char* nullPointer = nullptr;
    RequireInvalidArgument([&] { ASN1_get_object_nid_postfix(&nullPointer, &length, &tag, &objectClass, 2); });
}

}

int main() {
    ShortSequence();
    LongFormLength();
    IndefiniteLength();
    PrimitiveInteger();
    ExplicitClass();
    IncompleteHeader();
    Tags();
    TooLongLength();
    ZeroMaximum();
    IndefinitePrimitive();
    NullArguments();
    std::puts("libSceLibreSsl3 ASN1_get_object tests passed");
}

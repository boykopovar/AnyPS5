#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ASN.1 header check failed at line %u\n", location.line());
    std::abort();
}

std::vector<unsigned char> WithLength(std::size_t length) {
    std::vector<unsigned char> data{0x04, 0x82, static_cast<unsigned char>(length >> 8), static_cast<unsigned char>(length)};
    data.resize(data.size() + length);
    return data;
}

void Expect(const std::vector<unsigned char>& data, std::int64_t max, int result, std::int64_t length, int tag, int objectClass, std::size_t offset) {
    const unsigned char* cursor = data.data();
    std::int64_t parsedLength = -1;
    int parsedTag = -1;
    int parsedClass = -1;
    Require(ASN1_get_object_nid_postfix(&cursor, &parsedLength, &parsedTag, &parsedClass, max) == result);
    Require(parsedLength == length);
    Require(parsedTag == tag);
    Require(parsedClass == objectClass);
    Require(cursor == data.data() + offset);
}

void Parse() {
    Expect({0x02, 0x03, 0x01, 0x02, 0x03}, 5, 0, 3, 2, 0, 2);
    Expect({0x30, 0x80}, 2, 0x21, 0, 16, 0, 2);
    Expect({0x42, 0x00}, 2, 0, 0, 2, 0x40, 2);
    Expect({0xc2, 0x00}, 2, 0, 0, 2, 0xc0, 2);
    Expect({0x1f, 0x81, 0x00, 0x00}, 4, 0, 0, 128, 0, 4);
    Expect({0x3f, 0x81, 0x00, 0x00}, 4, 0x20, 0, 128, 0, 4);
    Expect({0x04, 0x82, 0x00, 0x00}, 4, 0, 0, 4, 0, 4);
    Expect(WithLength(256), 260, 0, 256, 4, 0, 4);
    Expect(WithLength(256), 4, 0x80, 256, 4, 0, 4);
    Require(ERR_get_error_nid_postfix() == 0x0dfff09b);
}

void Errors() {
    const std::vector<unsigned char> truncated{0x02};
    Expect(truncated, 1, 0x80, -1, -1, -1, 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const std::vector<unsigned char> truncatedLength{0x04, 0x82, 0x01};
    Expect(truncatedLength, 3, 0x80, -1, 4, 0, 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const std::vector<unsigned char> indefinitePrimitive{0x02, 0x80};
    Expect(indefinitePrimitive, 2, 0x80, 0, 2, 0, 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const std::vector<unsigned char> highTagOverflow{0x1f, 0xff, 0xff, 0xff, 0xff, 0x7f};
    Expect(highTagOverflow, 6, 0x80, -1, -1, -1, 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const std::vector<unsigned char> shortHighTag{0x1f, 0x81, 0x00};
    Expect(shortHighTag, 3, 0x80, -1, -1, -1, 0);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const unsigned char* cursor = nullptr;
    std::int64_t length = -1;
    int tag = -1;
    int objectClass = -1;
    Require(ASN1_get_object_nid_postfix(&cursor, &length, &tag, &objectClass, 0) == 0x80);
    Require(ERR_get_error_nid_postfix() == 0x0dfff07b);

    const unsigned char fiveByteLength[] = {0x04, 0x85, 0x00, 0x00, 0x00, 0x00, 0x05, 1, 2, 3, 4, 5};
    cursor = fiveByteLength;
    length = -1;
    tag = -1;
    objectClass = -1;
    Require(ASN1_get_object_nid_postfix(&cursor, &length, &tag, &objectClass, 12) == 0);
    Require(length == 5 && tag == 4 && objectClass == 0 && cursor == fiveByteLength + 7);

    const unsigned char longLength[] = {0x04, 0x85, 0x01, 0x00, 0x00, 0x00, 0x00};
    cursor = longLength;
    length = 0x5A5A5A5A5A5A5A5A;
    tag = -1;
    objectClass = -1;
    Require(ASN1_get_object_nid_postfix(&cursor, &length, &tag, &objectClass, 7) == 0x80);
    Require(length == 0x100000000LL && tag == 4 && objectClass == 0 && cursor == longLength + 7);
    Require(ERR_get_error_nid_postfix() == 0x0dfff09b);

    const std::vector<unsigned char> empty{0x02};
    bool threw = false;
    try {
        ASN1_get_object_nid_postfix(nullptr, &length, &tag, &objectClass, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    const unsigned char* pointer = empty.data();
    try {
        ASN1_get_object_nid_postfix(&pointer, nullptr, &tag, &objectClass, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        ASN1_get_object_nid_postfix(&pointer, &length, &tag, &objectClass, -1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    Parse();
    Errors();
    Require(ERR_get_error_nid_postfix() == 0);
    std::puts("libSceLibreSsl3 ASN.1 header tests passed");
}

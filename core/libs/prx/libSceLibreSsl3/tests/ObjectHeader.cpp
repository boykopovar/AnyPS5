#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ASN1 object header check failed at line %u\n", location.line());
    std::abort();
}

void Check(std::span<const unsigned char> bytes, int result, std::size_t consumed,
           std::int64_t length, int tag, int tagClass, int reason = 0) {
    const auto* input = bytes.data();
    std::array<std::int64_t, 3> lengths{11, 22, 33};
    std::array<int, 4> tags{44, 55, 66, 77};
    ERR_clear_error_nid_postfix();
    Require(ASN1_get_object_nid_postfix(&input, &lengths[1], &tags[1], &tags[2], bytes.size()) == result);
    Require(input == (consumed ? bytes.data() + consumed : bytes.data()));
    Require(lengths == std::array<std::int64_t, 3>{11, length, 33});
    Require(tags == std::array<int, 4>{44, tag, tagClass, 77});
    Require(ERR_get_error_nid_postfix() == (reason ? 0x0dfff000ULL + reason : 0));
    Require(ERR_get_error_nid_postfix() == 0);
}

}

int main() {
    for (int identifier = 0; identifier < 256; ++identifier) {
        if ((identifier & 31) == 31) continue;
        for (int length = 0; length < 128; ++length) {
            std::vector<unsigned char> bytes(length + 2);
            bytes[0] = static_cast<unsigned char>(identifier);
            bytes[1] = static_cast<unsigned char>(length);
            Check(bytes, identifier & 32, 2, length, identifier & 31, identifier & 192);
            if (length != 0) {
                bytes.pop_back();
                Check(bytes, 128 | (identifier & 32), 2, length, identifier & 31, identifier & 192, 155);
            }
        }
    }
    Check({}, 128, 0, 0, 0, 0, 123);
    for (const auto& bytes : {std::vector<unsigned char>{0x30}, {0x1f}, {0x1f, 0x80},
             {0x04, 0x80}, {0x30, 0xff}, {0x30, 0x82, 0},
             {0x1f, 0x88, 0x80, 0x80, 0x80, 0, 0},
             {0x1f, 0x90, 0x80, 0x80, 0x80, 0, 0},
             {0x30, 0x89, 1, 0, 0, 0, 0, 0, 0, 0, 0}})
        Check(bytes, 128, 0, 0, 0, 0, 123);
    Check(std::array<unsigned char, 2>{0x30, 0x80}, 33, 2, 0, 16, 0);
    Check(std::array<unsigned char, 3>{0x1f, 0, 0}, 0, 3, 0, 0, 0);
    Check(std::array<unsigned char, 4>{0xdf, 0x80, 0x7f, 0}, 0, 4, 0, 127, 192);
    Check(std::array<unsigned char, 7>{0x1f, 0x87, 0xff, 0xff, 0xff, 0x7f, 0}, 0, 7, 0, 0x7fffffff, 0);
    Check(std::array<unsigned char, 4>{0x04, 0x82, 0, 0}, 0, 4, 0, 4, 0);
    Check(std::array<unsigned char, 10>{0x30, 0x88, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        160, 10, -1, 16, 0, 155);
    std::vector<unsigned char> longest(128);
    longest[0] = 4;
    longest[1] = 0xfe;
    Check(longest, 0, 128, 0, 4, 0);
    for (int fault = 0; fault < 6; ++fault) {
        const unsigned char bytes[]{4, 0};
        const unsigned char* input = fault == 5 ? nullptr : bytes;
        std::int64_t length = 123;
        int tag = 456;
        int tagClass = 789;
        bool threw = false;
        try {
            ASN1_get_object_nid_postfix(fault == 0 ? nullptr : &input,
                fault == 1 ? nullptr : &length, fault == 2 ? nullptr : &tag,
                fault == 3 ? nullptr : &tagClass, fault == 4 ? -1 : 2);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw && length == 123 && tag == 456 && tagClass == 789);
    }
    const unsigned char bytes[]{0xc2, 0};
    const unsigned char* input = bytes;
    std::int64_t length;
    int shared;
    Require(ASN1_get_object_nid_postfix(&input, &length, &shared, &shared, 2) == 0);
    Require(shared == 2 && length == 0 && input == bytes + 2);
    LibreSslPutError_nid_no_patch(1, 2, 3);
    input = bytes;
    Require(ASN1_get_object_nid_postfix(&input, &length, &shared, &shared, 2) == 0);
    Require(ERR_get_error_nid_postfix() == 0x01002003);
}

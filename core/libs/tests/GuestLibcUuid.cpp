#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "uuid check failed\n");
        std::abort();
    }
}

static const char* const Sample = "01234567-89ab-cdef-0123-456789abcdef";

static GuestUuid Parse(const char* text, std::uint32_t expectedStatus) {
    GuestUuid uuid{};
    std::uint32_t status = 0xffffffffu;
    uuid_from_string_nid_postfix(text, &uuid, &status);
    Require(status == expectedStatus);
    return uuid;
}

int main() {
    const GuestUuid sample = Parse(Sample, 0);
    Require(sample.time_low == 0x01234567u && sample.time_mid == 0x89abu && sample.time_hi_and_version == 0xcdefu);
    Require(sample.clock_seq_hi_and_reserved == 0x01 && sample.clock_seq_low == 0x23);
    const std::uint8_t node[6] = {0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    Require(std::memcmp(sample.node, node, sizeof(node)) == 0);

    char* text = nullptr;
    std::uint32_t status = 0xffffffffu;
    uuid_to_string_nid_postfix(&sample, &text, &status);
    Require(status == 0 && text != nullptr);
    Require(std::strcmp(text, Sample) == 0);
    free_nid_postfix(text);

    GuestUuid nil{};
    uuid_create_nil_nid_postfix(&nil, &status);
    Require(status == 0 && uuid_is_nil_nid_postfix(&nil, nullptr) == 1);
    Require(uuid_is_nil_nid_postfix(nullptr, nullptr) == 1);
    Require(uuid_is_nil_nid_postfix(&sample, nullptr) == 0);

    text = nullptr;
    uuid_to_string_nid_postfix(nullptr, &text, nullptr);
    Require(std::strcmp(text, "00000000-0000-0000-0000-000000000000") == 0);
    free_nid_postfix(text);

    Require(uuid_hash_nid_postfix(&sample, nullptr) == 0x4567);
    Require(uuid_hash_nid_postfix(nullptr, nullptr) == 0);

    const std::uint8_t be[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    std::uint8_t encoded[16];
    uuid_enc_be_nid_postfix(encoded, &sample);
    Require(std::memcmp(encoded, be, sizeof(be)) == 0);
    GuestUuid decoded{};
    uuid_dec_be_nid_postfix(encoded, &decoded);
    Require(uuid_equal_nid_postfix(&decoded, &sample, nullptr) == 1);

    const std::uint8_t le[16] = {0x67, 0x45, 0x23, 0x01, 0xab, 0x89, 0xef, 0xcd, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    uuid_enc_le_nid_postfix(encoded, &sample);
    Require(std::memcmp(encoded, le, sizeof(le)) == 0);
    uuid_dec_le_nid_postfix(encoded, &decoded);
    Require(uuid_equal_nid_postfix(&decoded, &sample, nullptr) == 1);

    const GuestUuid variantBad = Parse("01234567-89ab-cdef-f123-456789abcdef", 1);
    Require(variantBad.clock_seq_hi_and_reserved == 0xf1);
    Parse("01234567-89ab-cdef-0123-456789abcde", 2);
    GuestUuid untouched{};
    uuid_from_string_nid_postfix("0123456789ab.cd.ef.01.23.45.67.89.ab", &untouched, &status);
    Require(status == 2);
    Require(uuid_is_nil_nid_postfix(&untouched, nullptr) == 1);

    GuestUuid empty = Parse("", 0);
    Require(uuid_is_nil_nid_postfix(&empty, nullptr) == 1);

    Require(uuid_compare_nid_postfix(nullptr, nullptr, nullptr) == 0);
    Require(uuid_compare_nid_postfix(nullptr, &sample, nullptr) == -1);
    Require(uuid_compare_nid_postfix(&sample, nullptr, nullptr) == 1);
    GuestUuid later = sample;
    later.time_low += 1;
    Require(uuid_compare_nid_postfix(&sample, &later, nullptr) == -1);
    Require(uuid_compare_nid_postfix(&later, &sample, nullptr) == 1);
    Require(uuid_compare_nid_postfix(&sample, &decoded, nullptr) == 0);

    Require(uuid_equal_nid_postfix(nullptr, &nil, nullptr) == 1);
    Require(uuid_equal_nid_postfix(&sample, &later, nullptr) == 0);
    return 0;
}

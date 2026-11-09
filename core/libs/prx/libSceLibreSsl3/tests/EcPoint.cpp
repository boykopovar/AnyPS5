#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {

constexpr int Compressed = 2;
constexpr int Uncompressed = 4;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC_POINT_dup check failed at line %u\n", location.line());
    std::abort();
}

std::vector<unsigned char> Encode(const EcGroup* group, const EcPoint* point, int form) {
    const std::size_t size = EC_POINT_point2oct_nid_postfix(group, point, form, nullptr, 0, nullptr);
    Require(size != 0);
    std::vector<unsigned char> encoded(size);
    Require(EC_POINT_point2oct_nid_postfix(group, point, form, encoded.data(), encoded.size(), nullptr) == size);
    return encoded;
}

void RequireDuplicate(const EcGroup* group, EcPoint* source) {
    const std::vector<unsigned char> uncompressed = Encode(group, source, Uncompressed);
    const std::vector<unsigned char> compressed = Encode(group, source, Compressed);
    EcPoint* copy = EC_POINT_dup_nid_postfix(source, group);
    Require(copy != nullptr);
    Require(copy != source);
    Require(Encode(group, copy, Uncompressed) == uncompressed);
    Require(Encode(group, copy, Compressed) == compressed);
    EC_POINT_free_nid_postfix(source);
    Require(Encode(group, copy, Uncompressed) == uncompressed);
    Require(Encode(group, copy, Compressed) == compressed);
    EcPoint* second = EC_POINT_dup_nid_postfix(copy, group);
    Require(second != nullptr && second != copy);
    EC_POINT_free_nid_postfix(copy);
    Require(Encode(group, second, Uncompressed) == uncompressed);
    EC_POINT_free_nid_postfix(second);
}

void RequireGenerator() {
    constexpr std::array<unsigned char, 65> generator{
        0x04,
        0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
        0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
        0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
        0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
    };
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    Require(group != nullptr);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Require(EC_POINT_oct2point_nid_postfix(group, point, generator.data(), generator.size(), nullptr) == 1);
    EcPoint* copy = EC_POINT_dup_nid_postfix(point, group);
    Require(copy != nullptr && copy != point);
    EC_POINT_free_nid_postfix(point);
    const std::vector<unsigned char> encoded = Encode(group, copy, Uncompressed);
    Require(encoded.size() == generator.size());
    Require(std::vector<unsigned char>(generator.begin(), generator.end()) == encoded);
    EC_POINT_free_nid_postfix(copy);
    EC_GROUP_free_nid_postfix(group);
}

void RequireInfinity(int nid) {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(nid);
    Require(group != nullptr);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    constexpr unsigned char infinity = 0;
    Require(EC_POINT_oct2point_nid_postfix(group, point, &infinity, 1, nullptr) == 1);
    EcPoint* copy = EC_POINT_dup_nid_postfix(point, group);
    Require(copy != nullptr && copy != point);
    EC_POINT_free_nid_postfix(point);
    Require(Encode(group, copy, Uncompressed) == std::vector<unsigned char>{0});
    EC_POINT_free_nid_postfix(copy);
    EC_GROUP_free_nid_postfix(group);
}

void RequireGenerated(int nid) {
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(nid);
    Require(key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    const EcPoint* publicKey = EC_KEY_get0_public_key_nid_postfix(key);
    Require(group != nullptr && publicKey != nullptr);
    EcPoint* source = EC_POINT_dup_nid_postfix(publicKey, group);
    Require(source != nullptr && source != publicKey);
    Require(Encode(group, source, Uncompressed) == Encode(group, publicKey, Uncompressed));
    EcGroup* separate = EC_GROUP_new_by_curve_name_nid_postfix(nid);
    Require(separate != nullptr);
    EC_KEY_free_nid_postfix(key);
    RequireDuplicate(separate, source);
    EC_GROUP_free_nid_postfix(separate);
}

} // namespace

int main() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    Require(group != nullptr);
    Require(EC_POINT_dup_nid_postfix(nullptr, group) == nullptr);
    Require(EC_POINT_dup_nid_postfix(nullptr, nullptr) == nullptr);
    EcPoint* point = EC_POINT_new_nid_postfix(group);
    Require(point != nullptr);
    Require(EC_POINT_dup_nid_postfix(point, nullptr) == nullptr);
    EC_POINT_free_nid_postfix(point);
    EC_GROUP_free_nid_postfix(group);
    RequireGenerator();
    constexpr std::array nids{409, 415, 711, 712, 713, 714, 715, 716, 928, 932, 934};
    for (const int nid : nids) {
        RequireInfinity(nid);
        RequireGenerated(nid);
    }
    std::puts("LibreSSL3 EC_POINT_dup tests passed");
    return 0;
}

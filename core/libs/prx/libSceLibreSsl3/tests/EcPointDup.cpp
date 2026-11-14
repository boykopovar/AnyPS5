#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EC_POINT_dup check failed at line %u\n", location.line());
    std::abort();
}

std::array<unsigned char, 65> Encoding(const EcGroup* group, const EcPoint* point) {
    std::array<unsigned char, 65> buffer {};
    Require(EC_POINT_point2oct_nid_postfix(group, point, 4, buffer.data(), buffer.size(), nullptr) == buffer.size());
    return buffer;
}

}

int main() {
    EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
    EcGroup* other = EC_GROUP_new_by_curve_name_nid_postfix(715);
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(group != nullptr && other != nullptr && key != nullptr);
    Require(EC_KEY_generate_key_nid_postfix(key) == 1);
    const EcPoint* source = EC_KEY_get0_public_key_nid_postfix(key);
    Require(source != nullptr);

    EcPoint* copy = EC_POINT_dup_nid_postfix(source, group);
    Require(copy != nullptr && copy != source);
    Require(Encoding(group, copy) == Encoding(group, source));
    EC_POINT_clear_free_nid_postfix(copy);

    Require(EC_POINT_dup_nid_postfix(nullptr, group) == nullptr);
    Require(EC_POINT_dup_nid_postfix(source, other) == nullptr);

    bool threw = false;
    try {
        EC_POINT_dup_nid_postfix(source, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    EC_KEY_free_nid_postfix(key);
    EC_GROUP_free_nid_postfix(other);
    EC_GROUP_free_nid_postfix(group);
    std::puts("libSceLibreSsl3 EC_POINT_dup tests passed");
}

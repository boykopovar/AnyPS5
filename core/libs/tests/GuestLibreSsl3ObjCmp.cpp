#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>

extern "C" int APS5_VABI OBJ_cmp_nid_postfix(const void*, const void*);

namespace {
struct Object {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::terminate();
}

int Sign(int value) {
    return value < 0 ? -1 : value > 0 ? 1 : 0;
}
}

int main() {
    const unsigned char short1[] = {0x2a, 0x86, 0x48};
    const unsigned char same1[] = {0x2a, 0x86, 0x48};
    const unsigned char low[] = {0x2a, 0x86, 0x47};
    const unsigned char high[] = {0x2a, 0x86, 0x49};
    const unsigned char longer[] = {0x2a, 0x86, 0x48, 0x01};
    const unsigned char first[] = {0x01, 0xff, 0xff};
    const unsigned char second[] = {0x02, 0x00, 0x00};
    const Object a{nullptr, nullptr, 0, 3, short1, 0};
    const Object b{nullptr, nullptr, 0, 3, same1, 0};
    const Object c{nullptr, nullptr, 0, 3, low, 0};
    const Object d{nullptr, nullptr, 0, 3, high, 0};
    const Object e{nullptr, nullptr, 0, 4, longer, 0};
    const Object f{nullptr, nullptr, 0, 3, first, 0};
    const Object g{nullptr, nullptr, 0, 3, second, 0};
    const Object emptyA{nullptr, nullptr, 0, 0, nullptr, 0};
    const Object emptyB{nullptr, nullptr, 0, 0, short1, 0};
    Require(OBJ_cmp_nid_postfix(&a, &a) == 0 && OBJ_cmp_nid_postfix(&a, &b) == 0, "equal encodings compare equal");
    Require(OBJ_cmp_nid_postfix(&a, &c) == 1 && OBJ_cmp_nid_postfix(&c, &a) == -1, "the first differing byte decides, as its difference");
    Require(OBJ_cmp_nid_postfix(&a, &d) == -1 && OBJ_cmp_nid_postfix(&d, &a) == 1, "a higher byte compares greater");
    Require(OBJ_cmp_nid_postfix(&a, &e) == -1 && OBJ_cmp_nid_postfix(&e, &a) == 1, "the length difference is returned before the bytes");
    Require(Sign(OBJ_cmp_nid_postfix(&f, &g)) == -1 && OBJ_cmp_nid_postfix(&f, &g) == -1, "bytes are compared as unsigned values");
    Require(OBJ_cmp_nid_postfix(&emptyA, &emptyB) == 0, "empty encodings compare equal without reading data");
    auto throws = [](const void* left, const void* right) {
        try {
            static_cast<void>(OBJ_cmp_nid_postfix(left, right));
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };
    Require(throws(nullptr, &a) && throws(&a, nullptr), "null objects are rejected");
    const Object missing{nullptr, nullptr, 0, 3, nullptr, 0};
    const Object negative{nullptr, nullptr, 0, -1, short1, 0};
    const Object empty{nullptr, nullptr, 0, 0, nullptr, 0};
    const Object minimum{nullptr, nullptr, 0, std::numeric_limits<int>::min(), nullptr, 0};
    const Object one{nullptr, nullptr, 0, 1, short1, 0};
    Require(throws(&missing, &missing) && throws(&negative, &negative) && throws(&negative, &empty), "missing data and negative lengths are rejected");
    Require(throws(&minimum, &one), "extreme negative lengths are rejected without overflowing");
    return 0;
}

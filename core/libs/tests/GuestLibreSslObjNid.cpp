#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <exception>

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
}

int main() {
    Require(OBJ_obj2nid_nid_postfix(nullptr) == 0, "a null object has NID_undef");
    static constexpr unsigned char encoded[] = {0x2a, 0x86, 0x48};
    Object known{nullptr, nullptr, 672, 9, encoded, 0};
    Require(OBJ_obj2nid_nid_postfix(&known) == 672, "an object with a NID returns it without reading the bytes");
    Object empty{nullptr, nullptr, 0, 0, nullptr, 0};
    Require(OBJ_obj2nid_nid_postfix(&empty) == 404, "an empty object follows LibreSSL 2.5.4 table search to NID_ccitt");
    return 0;
}

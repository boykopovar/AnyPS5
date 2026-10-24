#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <exception>

extern "C" int APS5_VABI OBJ_obj2nid_nid_postfix(const void* object);

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
    Require(OBJ_obj2nid_nid_postfix(&empty) == 0, "an object without a NID or bytes has NID_undef");
    Object unnamed{nullptr, nullptr, 0, 3, encoded, 0};
    try {
        static_cast<void>(OBJ_obj2nid_nid_postfix(&unnamed));
    } catch (const std::exception&) {
        return 0;
    }
    Require(false, "an object known only by its bytes is rejected");
    return 1;
}

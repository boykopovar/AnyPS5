#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {
struct Object {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};
static_assert(sizeof(Object) == 40 && offsetof(Object, length) == 20 && offsetof(Object, data) == 24);
}

extern "C" int APS5_VABI OBJ_cmp_nid_postfix(const void* first, const void* second) {
    if (!first || !second) throw std::invalid_argument(std::string(__func__) + ": null object");
    const auto* left = static_cast<const Object*>(first);
    const auto* right = static_cast<const Object*>(second);
    if (const int difference = left->length - right->length) return difference;
    if (left->length < 0 || (left->length > 0 && (!left->data || !right->data))) throw std::invalid_argument(std::string(__func__) + ": invalid encoded data");
    for (int index = 0; index < left->length; ++index) {
        if (left->data[index] != right->data[index]) return static_cast<int>(left->data[index]) - static_cast<int>(right->data[index]);
    }
    return 0;
}

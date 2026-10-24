#include <cstddef>
#include "prx/libc/include/General.hpp"
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
static_assert(sizeof(Object) == 40 && offsetof(Object, nid) == 16 && offsetof(Object, length) == 20);
}

extern "C" int APS5_VABI OBJ_obj2nid_nid_postfix(const void* value) {
    if (!value) return 0;
    const auto* object = static_cast<const Object*>(value);
    if (object->nid != 0) return object->nid;
    if (object->length == 0) return 0;
    NotImplemented_nid_no_patch("OBJ_obj2nid lookup of an object identifier by its encoded bytes");
    return 0;
}

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int UndefinedType = -1;
constexpr std::int64_t MultiStringFlag = 0x040;

}

extern "C" {

Asn1String* APS5_VABI ASN1_TIME_new_nid_postfix() {
    auto* time = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!time) {
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, MallocFailure);
        return nullptr;
    }
    time->type = UndefinedType;
    time->flags = MultiStringFlag;
    return time;
}

}

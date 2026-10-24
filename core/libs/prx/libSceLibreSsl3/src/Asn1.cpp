#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslAsn1String {
    int length;
    int type;
    unsigned char* data;
    std::int64_t flags;
};

static_assert(sizeof(LibreSslAsn1String) == 24);
static_assert(offsetof(LibreSslAsn1String, type) == 4);
static_assert(offsetof(LibreSslAsn1String, data) == 8);
static_assert(offsetof(LibreSslAsn1String, flags) == 16);

extern "C" {

const unsigned char* APS5_VABI ASN1_STRING_get0_data_nid_postfix(const LibreSslAsn1String* string) {
    if (!string) throw std::invalid_argument(std::string(__func__) + ": null string");
    return string->data;
}

}

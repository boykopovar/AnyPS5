#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int64_t Asn1StringFlagNdef = 0x010;

}

extern "C" {
const unsigned char* APS5_VABI ASN1_STRING_get0_data_nid_postfix(const Asn1String* string) {
    if (!string) throw std::invalid_argument(std::string(__func__) + ": null string");
    return string->data;
}

void APS5_VABI ASN1_INTEGER_free_nid_postfix(Asn1String* integer) {
    if (!integer) return;
    if (!(integer->flags & Asn1StringFlagNdef) && integer->data) {
        if (integer->length < 0) throw std::invalid_argument(std::string(__func__) + ": negative length");
        std::memset(integer->data, 0, static_cast<std::size_t>(integer->length));
        free_nid_postfix(integer->data);
    }
    free_nid_postfix(integer);
}

}

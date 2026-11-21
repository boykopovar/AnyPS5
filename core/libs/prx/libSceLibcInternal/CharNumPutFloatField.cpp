#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const char* digits, std::size_t count) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) throw std::invalid_argument("num_put<char>::_Fput: a stream without a locale");
    if (count != 0 && digits == nullptr) throw std::invalid_argument("num_put<char>::_Fput: a null digit buffer");
    std::string text(count, '\0');
    if (count != 0 && !GuestCopyIn_nid_no_patch(text.data(), digits, count)) throw std::invalid_argument("num_put<char>::_Fput: an unreadable digit buffer");
    return LibcNumPutFloat_nid_no_patch(out, stream, fill, text.c_str(), count);
}

}

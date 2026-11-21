#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t WideChunk = 256;

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, std::uint16_t element, std::size_t count) {
    return LibcRepeatWideElement_nid_no_patch(out, element, count);
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_PKwm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, const std::uint16_t* elements, std::size_t count) {
    std::array<std::uint16_t, WideChunk> chunk{};
    while (count != 0) {
        const std::size_t size = std::min(count, WideChunk);
        if (!GuestCopyIn_nid_no_patch(chunk.data(), elements, size * sizeof(std::uint16_t))) throw std::invalid_argument("num_put<wchar_t>::_Put: an unreadable source");
        out = LibcPutWideElements_nid_no_patch(out, chunk.data(), size);
        elements += size;
        count -= size;
    }
    return out;
}

GuestLocale::OutputIterator APS5_VABI _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IputES3_RSt8ios_basecPcm_nid_postfix(
    const GuestLocale::Facet*, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, char* digits, std::size_t count) {
    if (stream == nullptr || stream->locale == nullptr || *stream->locale == nullptr) throw std::invalid_argument("num_put<char>::_Iput: a stream without a locale");
    std::string text(count, '\0');
    if (count != 0 && !GuestCopyIn_nid_no_patch(text.data(), digits, count)) throw std::invalid_argument("num_put<char>::_Iput: an unreadable digit buffer");
    return LibcNumPutInteger_nid_no_patch(out, stream, fill, text.data(), count);
}

}

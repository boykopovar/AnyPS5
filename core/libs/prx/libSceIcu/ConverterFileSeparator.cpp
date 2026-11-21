#include "prx/libSceIcu/ConverterFileSeparator.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>

extern "C" {

void APS5_VABI ucnv_fixFileSeparator_nid_postfix(const Icu::Converter* converter, char16_t* source, std::int32_t length) {
    const int savedErrno = errno;
    if (!converter || !source || length <= 0) {
        errno = savedErrno;
        return;
    }
    const auto* native = reinterpret_cast<const UConverter*>(converter);
    if (!ucnv_isAmbiguous(native)) {
        errno = savedErrno;
        return;
    }
    std::array<char16_t, 1024> buffer{};
    std::int32_t offset = 0;
    while (offset < length) {
        const auto count = static_cast<std::size_t>(std::min<std::int32_t>(length - offset, buffer.size()));
        const auto bytes = count * sizeof(char16_t);
        if (!GuestCopyIn_nid_no_patch(buffer.data(), source + offset, bytes)) {
            errno = savedErrno;
            NotImplemented_nid_no_patch(__func__);
        }
        const auto initial = buffer;
        ucnv_fixFileSeparator(native, buffer.data(), static_cast<std::int32_t>(count));
        for (std::size_t first = 0; first < count;) {
            if (buffer[first] == initial[first]) {
                ++first;
                continue;
            }
            std::size_t last = first + 1;
            while (last < count && buffer[last] != initial[last]) ++last;
            if (!GuestCopyOut_nid_no_patch(source + offset + first, buffer.data() + first,
                                           (last - first) * sizeof(char16_t))) {
                errno = savedErrno;
                NotImplemented_nid_no_patch(__func__);
            }
            first = last;
        }
        offset += static_cast<std::int32_t>(count);
    }
    errno = savedErrno;
}

}

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

std::uint16_t Widen(char character) {
    return static_cast<std::uint16_t>(btowc_nid_postfix(static_cast<unsigned char>(character)));
}

char Narrow(std::uint16_t character, char fallback) {
    const int byte = wctob_nid_postfix(character);
    return byte == -1 ? fallback : static_cast<char>(byte);
}

}

extern "C" {

std::uint16_t APS5_VABI sceLibcInternalWideCtypeWiden_nid_no_patch(const void*, char character) {
    return Widen(character);
}
APS5_EXPORT("h3PbnNnZ-gI", sceLibcInternalWideCtypeWiden_nid_no_patch);

const char* APS5_VABI sceLibcInternalWideCtypeWidenRange_nid_no_patch(const void*, const char* first, const char* last, std::uint16_t* destination) {
    for (; first != last; ++first, ++destination) *destination = Widen(*first);
    return first;
}
APS5_EXPORT("KM0b6O8show", sceLibcInternalWideCtypeWidenRange_nid_no_patch);

char APS5_VABI sceLibcInternalWideCtypeNarrow_nid_no_patch(const void*, std::uint16_t character, char fallback) {
    return Narrow(character, fallback);
}
APS5_EXPORT("V+c0E+Uqcww", sceLibcInternalWideCtypeNarrow_nid_no_patch);

const std::uint16_t* APS5_VABI sceLibcInternalWideCtypeNarrowRange_nid_no_patch(const void*, const std::uint16_t* first, const std::uint16_t* last, char fallback, char* destination) {
    for (; first != last; ++first, ++destination) *destination = Narrow(*first, fallback);
    return first;
}
APS5_EXPORT("Vf68JAD5rbM", sceLibcInternalWideCtypeNarrowRange_nid_no_patch);

}

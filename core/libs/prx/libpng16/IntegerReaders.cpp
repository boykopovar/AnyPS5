#include "prx/libpng16/include/IntegerReaders.hpp"
#include <png.h>

extern "C" {

std::uint16_t APS5_VABI png_get_uint_16_nid_postfix(const std::uint8_t* bytes) {
    return png_get_uint_16(bytes);
}

std::uint32_t APS5_VABI png_get_uint_32_nid_postfix(const std::uint8_t* bytes) {
    return png_get_uint_32(bytes);
}

}

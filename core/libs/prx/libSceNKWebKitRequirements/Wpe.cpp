#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"

#include <wpe/input.h>

#include <cstdint>

extern "C" {

std::uint32_t APS5_VABI wpe_key_code_to_unicode_nid_postfix(std::uint32_t keyCode) {
    return wpe_key_code_to_unicode(keyCode);
}

std::uint32_t APS5_VABI wpe_unicode_to_key_code_nid_postfix(std::uint32_t character) {
    return wpe_unicode_to_key_code(character);
}

}

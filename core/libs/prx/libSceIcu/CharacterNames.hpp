#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
using CharacterNameVisitor = std::int8_t (APS5_VABI*)(void*, std::int32_t, std::int32_t, const char*, std::int32_t);

struct SetAdder {
    void* set;
    void (APS5_VABI* add)(void*, std::int32_t);
    void (APS5_VABI* addRange)(void*, std::int32_t, std::int32_t);
    void (APS5_VABI* addString)(void*, const char16_t*, std::int32_t);
    void (APS5_VABI* remove)(void*, std::int32_t);
    void (APS5_VABI* removeRange)(void*, std::int32_t, std::int32_t);
};
} // namespace Icu

extern "C" {
std::int32_t APS5_VABI u_charName_nid_postfix(std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_charFromName_nid_postfix(std::int32_t, const char*, std::int32_t*);
void APS5_VABI u_enumCharNames_nid_postfix(std::int32_t, std::int32_t, Icu::CharacterNameVisitor, void*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI uprv_getMaxCharNameLength_nid_postfix();
void APS5_VABI uprv_getCharNameCharacters_nid_postfix(const Icu::SetAdder*);
}

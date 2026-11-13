#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage6sourceEv(const void* self);
std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage4typeEv(const void* self);
std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage5levelEv(const void* self);
void APS5_VABI _ZN9Inspector14ConsoleMessage14incrementCountEv(void* self);
unsigned APS5_VABI _ZNK9Inspector14ConsoleMessage13argumentCountEv(const void* self);
void* APS5_VABI _ZNK9Inspector14ConsoleMessage12globalObjectEv(const void* self);
void* APS5_VABI _ZNK9Inspector15ScriptArguments12globalObjectEv(const void* self);
std::uint64_t APS5_VABI _ZNK9Inspector15ScriptArguments10argumentAtEm(const void* self, std::size_t index);

}

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfBase64Exports.hpp"

#include <cstddef>

namespace Jsc::WtfAbi {

struct SizeVector {
    std::size_t* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(std::size_t) == 8);
static_assert(sizeof(SizeVector) == 16);
static_assert(offsetof(SizeVector, buffer) == 0);
static_assert(offsetof(SizeVector, capacity) == 8);
static_assert(offsetof(SizeVector, size) == 12);

} // namespace Jsc::WtfAbi

extern "C" {

Jsc::WtfAbi::SizeVector* APS5_VABI _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(Jsc::WtfAbi::SizeVector* result, const Jsc::WtfAbi::String* text);

}

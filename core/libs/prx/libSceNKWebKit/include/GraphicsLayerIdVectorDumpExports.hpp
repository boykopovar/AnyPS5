#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include <cstddef>
#include <cstdint>

static_assert(sizeof(NKWebKit::WtfAbi::Vector<std::uint64_t>) == 16);
static_assert(offsetof(NKWebKit::WtfAbi::Vector<std::uint64_t>, capacity) == 8);
static_assert(offsetof(NKWebKit::WtfAbi::Vector<std::uint64_t>, size) == 12);

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(NKWebKit::WtfAbi::TextStream* stream, const NKWebKit::WtfAbi::Vector<std::uint64_t>* layers);

}

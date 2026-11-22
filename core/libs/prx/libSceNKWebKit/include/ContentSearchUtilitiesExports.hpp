#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>

namespace WTF {

struct TextPosition {
    int line;
    int column;
};

static_assert(sizeof(TextPosition) == 8);

}

extern "C" {
WTF::TextPosition APS5_VABI _ZN9Inspector22ContentSearchUtilities22textPositionFromOffsetEmRKN3WTF6VectorImLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::size_t offset, const NKWebKit::WtfAbi::Vector<std::size_t>& lineEndings);
}

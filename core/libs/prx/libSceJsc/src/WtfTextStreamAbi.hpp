#pragma once

#include <cstddef>

namespace Jsc::WtfAbi {

struct TextStream;

struct FormatNumberRespectingIntegers {
    double value;
};

static_assert(sizeof(FormatNumberRespectingIntegers) == 8);
static_assert(offsetof(FormatNumberRespectingIntegers, value) == 0);

} // namespace Jsc::WtfAbi

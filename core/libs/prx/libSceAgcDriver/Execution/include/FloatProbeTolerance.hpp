#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOATPROBETOLERANCE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_FLOATPROBETOLERANCE_HPP

#include <cstdint>

namespace AgcDriver {

enum class Float32ProbeOp : std::uint8_t { Log2, Sqrt, Rsq, Rcp, Exp2 };

bool Float32ResultsAgree(Float32ProbeOp op, std::uint32_t input, std::uint32_t first, std::uint32_t second);

}

#endif

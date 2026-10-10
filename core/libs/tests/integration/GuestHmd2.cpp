#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceHmd2Initialize(const void* param);
int APS5_VABI sceHmd2Open();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t notSupported = static_cast<std::int32_t>(0x81110016);

const Case initializeWithParam{"Initialize_ZeroedParam_ReportsUnsupportedFeature", [] {
    const std::uint8_t param[16]{};
    RequireEqual(sceHmd2Initialize(param), notSupported, "initialization with a param");
}};

const Case initializeWithoutParam{"Initialize_NullParam_ReportsUnsupportedFeature", [] {
    RequireEqual(sceHmd2Initialize(nullptr), notSupported, "initialization without a param");
}};

const Case openAfterFailedInit{"Open_AfterFailedInitialize_ThrowsRuntimeError", [] {
    const std::uint8_t param[16]{};
    sceHmd2Initialize(param);
    RequireThrows<std::runtime_error>([] { sceHmd2Open(); }, "open after failed initialization");
}};

} // namespace

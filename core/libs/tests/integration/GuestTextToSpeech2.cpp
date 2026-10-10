#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param);
int APS5_VABI sceTextToSpeech2Open();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t notSupported = static_cast<std::int32_t>(0x8002002D);

std::int32_t InitializeWithDefaultParam() {
    const std::uint32_t param[12]{0x2000000, 0x26c};
    return sceTextToSpeech2Initialize(param);
}

const Case initialize{"Initialize_ValidParam_ReportsUnsupportedOperation", [] {
    RequireEqual(InitializeWithDefaultParam(), notSupported, "initialization result");
}};

const Case openAfterFailedInit{"Open_AfterFailedInitialize_ThrowsRuntimeError", [] {
    InitializeWithDefaultParam();
    RequireThrows<std::runtime_error>([] { sceTextToSpeech2Open(); }, "open after failed initialization");
}};

} // namespace

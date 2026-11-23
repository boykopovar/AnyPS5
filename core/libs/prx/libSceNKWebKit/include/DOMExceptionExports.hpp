#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/DOMExceptionAbi.hpp"

#include <cstdint>

extern "C" {

const NKWebKit::WebCoreAbi::DOMExceptionDescription& APS5_VABI _ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(std::uint32_t code);

}

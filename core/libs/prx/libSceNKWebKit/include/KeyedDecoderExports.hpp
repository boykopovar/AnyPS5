#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/KeyedDecoderAbi.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

NKWebKit::WebCoreAbi::KeyedDecoderPtr APS5_VABI _ZN7WebCore12KeyedDecoder7decoderEPKhm(const std::uint8_t* data, std::size_t size);

}

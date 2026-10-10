#include <Testing/Test.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

const Case vgprLoad{"Decode_BufferLoadDwordWithoutLds_DecodesAsBufferLoadDword", [] {
    const std::array<std::uint32_t, 3> load{0xe0300000u, 0x80010100u, 0xbf810000u};
    const auto program = RdnaInstructionDecoder{}.Decode(std::span(load));
    Require(!program.instructions.empty() && program.instructions[0].op == RdnaOpcode::BufferLoadDword,
            "buffer_load_dword v1, off, s[4:7], 0 does not decode as BufferLoadDword");
}};

const Case ldsLoad{"Decode_BufferLoadDwordWithLds_IsRejected", [] {
    const std::array<std::uint32_t, 3> load{0xe0310000u, 0x80010100u, 0xbf810000u};
    const auto error = Testing::RequireThrows<std::runtime_error>([&] { static_cast<void>(RdnaInstructionDecoder{}.Decode(std::span(load))); },
                                                                  "buffer_load_dword off, s[4:7], 0 lds decodes as a VGPR load");
    Require(std::string(error.what()).find("unsupported MUBUF lds modifier") != std::string::npos,
            std::string("buffer_load_dword off, s[4:7], 0 lds: ") + error.what());
}};

} // namespace

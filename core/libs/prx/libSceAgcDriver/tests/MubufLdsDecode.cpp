#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
using namespace ShaderRecompiler;
int main() {
    int failures = 0;
    const std::array<std::uint32_t, 3> load{0xe0300000u, 0x80010100u, 0xbf810000u};
    try {
        const auto program = RdnaInstructionDecoder{}.Decode(std::span(load));
        if (program.instructions.empty() || program.instructions[0].op != RdnaOpcode::BufferLoadDword) {
            std::fprintf(stderr, "buffer_load_dword v1, off, s[4:7], 0 does not decode as BufferLoadDword\n");
            ++failures;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "buffer_load_dword v1, off, s[4:7], 0: %s\n", error.what());
        ++failures;
    }
    const std::array<std::uint32_t, 3> ldsLoad{0xe0310000u, 0x80010100u, 0xbf810000u};
    try {
        static_cast<void>(RdnaInstructionDecoder{}.Decode(std::span(ldsLoad)));
        std::fprintf(stderr, "buffer_load_dword off, s[4:7], 0 lds decodes as a VGPR load\n");
        ++failures;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("unsupported MUBUF lds modifier") == std::string::npos) {
            std::fprintf(stderr, "buffer_load_dword off, s[4:7], 0 lds: %s\n", error.what());
            ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}

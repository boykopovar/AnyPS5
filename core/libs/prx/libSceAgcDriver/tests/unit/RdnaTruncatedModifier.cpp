#include <Testing/Test.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::RequireEqual;

struct Encoding {
    const char* name;
    std::uint32_t word;
};

void RequireTruncationRejected(const Encoding& entry) {
    const std::array<std::uint32_t, 2> words{entry.word, 0xaf00e400u};
    const auto error = Testing::RequireThrows<std::out_of_range>([&] { static_cast<void>(RdnaInstructionDecoder{}.Decode(std::span(words).first(1))); },
                                                                 std::string(entry.name) + ": a missing modifier word decodes");
    RequireEqual(std::string_view(error.what()), std::string("truncated ") + entry.name + " instruction", entry.name);
}

const Case sdwa{"Decode_SdwaWithoutModifierWord_ThrowsTruncated", [] {
    for (const auto& entry : std::array<Encoding, 3>{{{"VOP1 SDWA", 0x7e0202f9u}, {"VOP2 SDWA", 0x4a0202f9u}, {"VOPC SDWA", 0x7c0202f9u}}}) {
        RequireTruncationRejected(entry);
    }
}};

const Case dpp{"Decode_DppWithoutModifierWord_ThrowsTruncated", [] {
    for (const auto& entry : std::array<Encoding, 3>{{{"VOP1 DPP", 0x7e0202fau}, {"VOP2 DPP", 0x4a0202fau}, {"VOPC DPP", 0x7c0202fau}}}) {
        RequireTruncationRejected(entry);
    }
}};

} // namespace

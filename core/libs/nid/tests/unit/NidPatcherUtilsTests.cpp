#include <Testing/Test.hpp>
#include <nid/ExportExclusions.hpp>
#include <nid/NidPatcherUtils.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Nid::Internal;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

const Case noPatchSuffix{"IsNidNoPatch_SuffixOrSdlPrefix_IsDetected", [] {
    Require(IsNidNoPatch("helper_nid_no_patch"), "suffix");
    Require(IsNidNoPatch("SDL_Quit"), "SDL prefix");
    Require(!IsNidNoPatch("printf"), "plain name");
    Require(!IsNidNoPatch("_nid_no_patc"), "partial suffix");
}};

const Case noPatchCutSuffix{"IsNidNoPatchCut_Suffix_IsDetectedAndStripped", [] {
    Require(IsNidNoPatchCut("memcpy_nid_no_patch_cut"), "suffix");
    Require(!IsNidNoPatchCut("memcpy_nid_no_patch"), "no-patch suffix only");
    RequireEqual(StripNidNoPatchCut("memcpy_nid_no_patch_cut"), std::string("memcpy"), "stripped name");
}};

const Case postfixStripping{"StripNidPostfix_Variants_StripOnlyRecognizedSuffixes", [] {
    RequireEqual(StripNidPostfix("printf_nid_postfix"), std::string("printf"), "postfix");
    RequireEqual(StripNidPostfix("printf_nid_disambig3"), std::string("printf"), "numeric marker");
    RequireEqual(StripNidPostfix("printf_nid_disambig3_nid_postfix"), std::string("printf"), "marker and postfix");
    RequireEqual(StripNidPostfix("printf_nid_disambig"), std::string("printf_nid_disambig"), "marker without digits");
    RequireEqual(StripNidPostfix("printf_nid_disambigX"), std::string("printf_nid_disambigX"), "marker with letters");
    RequireEqual(StripNidPostfix("_nid_postfix"), std::string(""), "postfix alone");
    RequireEqual(StripNidPostfix("printf"), std::string("printf"), "plain name");
}};

const Case readInBounds{"Read_ValueEndingAtBufferEnd_ReadsLittleEndianValue", [] {
    const std::vector<std::uint8_t> buffer{0xAA, 0x01, 0x02, 0x03, 0x04};
    RequireEqual(Read<std::uint32_t>(buffer, 1), std::uint32_t{0x04030201}, "unaligned read");
}};

const Case readOutOfBounds{"Read_ValuePastBufferEnd_Throws", [] {
    const std::vector<std::uint8_t> buffer(4);
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { Read<std::uint32_t>(buffer, 1); }, "read out of bounds", "read past end");
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { Read<std::uint8_t>(buffer, 4); }, "read out of bounds", "read at end");
}};

const Case writeInBounds{"Write_ValueEndingAtBufferEnd_WritesBytes", [] {
    std::vector<std::uint8_t> buffer(5);
    Write<std::uint32_t>(buffer, 1, 0x04030201u);
    RequireEqual(buffer, std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03, 0x04}, "written bytes");
}};

const Case writeOutOfBounds{"Write_ValuePastBufferEnd_ThrowsAndLeavesBufferUnchanged", [] {
    std::vector<std::uint8_t> buffer(4, 0xCC);
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { Write<std::uint32_t>(buffer, 2, 0u); }, "write out of bounds", "write past end");
    RequireEqual(buffer, std::vector<std::uint8_t>(4, 0xCC), "buffer unchanged");
}};

const Case cStringTerminated{"ReadCStr_TerminatedString_StopsAtNull", [] {
    const std::vector<std::uint8_t> buffer{'x', 'a', 'b', 0, 'c'};
    RequireEqual(ReadCStr(buffer, 1), std::string("ab"), "terminated string");
}};

const Case cStringUnterminated{"ReadCStr_UnterminatedString_StopsAtBufferEnd", [] {
    const std::vector<std::uint8_t> buffer{'a', 'b', 'c'};
    RequireEqual(ReadCStr(buffer, 1), std::string("bc"), "unterminated string");
}};

const Case cStringOutOfBounds{"ReadCStr_OffsetAtBufferEnd_Throws", [] {
    const std::vector<std::uint8_t> buffer{'a'};
    Testing::RequireThrowsWithMessage<std::runtime_error>([&] { ReadCStr(buffer, 1); }, "cstr offset out of bounds", "offset at end");
}};

const Case normalizeExportName{"NormalizeExportName_NidPostfix_IsRemoved", [] {
    RequireEqual(Nid::NormalizeExportName("printf_nid_postfix"), std::string("printf"), "postfix removed");
    RequireEqual(Nid::NormalizeExportName("printf"), std::string("printf"), "plain name kept");
    RequireEqual(Nid::NormalizeExportName("printf_nid_disambig2"), std::string("printf_nid_disambig2"), "marker kept");
}};

} // namespace

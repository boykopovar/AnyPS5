#include "prx/libSceVdecShevc/libSceVdecShevc.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

using HandleCall = decltype(&sceSdecExecuteSwHevc);

void Require(bool value) {
    if (!value) std::abort();
}

std::array<unsigned char, 0x40> Block() {
    std::array<unsigned char, 0x40> block{};
    block.fill(0x5A);
    return block;
}

void PutQuad(std::array<unsigned char, 0x40>& block, std::size_t offset, std::uint64_t value) {
    std::memcpy(block.data() + offset, &value, sizeof(value));
}

std::uint64_t GetQuad(const std::array<unsigned char, 0x40>& block, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, block.data() + offset, sizeof(value));
    return value;
}

void NoDecoderAnswers() {
    const std::array<HandleCall, 9> calls{
        sceSdecExecuteSwHevc,
        sceSdecGetFrameSwHevc,
        sceSdecResetSwHevc,
        libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch,
        libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch,
        libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch,
        libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch,
        libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch,
        libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch,
    };
    auto handle = Block();
    PutQuad(handle, 0, 0);
    const auto before = handle;
    for (const HandleCall call : calls) {
        for (int round = 0; round < 2; ++round) {
            Require(call(nullptr) == 1);
            Require(call(handle.data()) == 1);
        }
    }
    Require(handle == before);
}

void CreateRejects() {
    for (const std::uint32_t first : {0u, 2u, 0xFFFFFFFFu, 0x5A5A5A5Au}) {
        auto param = Block();
        std::memcpy(param.data(), &first, sizeof(first));
        const auto before = param;
        for (int round = 0; round < 2; ++round) Require(sceSdecCreateSwHevc(param.data()) == 3);
        Require(param == before);
    }
}

void DestroyWithoutDecoder() {
    for (int round = 0; round < 2; ++round) Require(sceSdecDestroySwHevc(nullptr) == 0);

    auto handle = Block();
    PutQuad(handle, 0x10, 0);
    auto expected = handle;
    PutQuad(expected, 0x8, 0);
    Require(sceSdecDestroySwHevc(handle.data()) == 0);
    Require(handle == expected);
    Require(GetQuad(handle, 0x8) == 0);
    Require(sceSdecDestroySwHevc(handle.data()) == 0);
    Require(handle == expected);

    auto cleared = Block();
    PutQuad(cleared, 0x8, 0);
    PutQuad(cleared, 0x10, 0);
    const auto clearedBefore = cleared;
    Require(sceSdecDestroySwHevc(cleared.data()) == 0);
    Require(cleared == clearedBefore);
}

} // namespace

int main() {
    NoDecoderAnswers();
    CreateRejects();
    DestroyWithoutDecoder();
    return 0;
}

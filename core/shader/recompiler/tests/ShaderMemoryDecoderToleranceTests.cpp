#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
        std::abort();
    }
}

template <typename T, typename U>
void ExpectEq(const T& actual, const U& expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "ExpectEq failed: %s\n", message);
        std::abort();
    }
}
#define EXPECT_EQ(a, b) ExpectEq((a), (b), #a " == " #b)

void ExpectThrows(const std::array<std::uint32_t, 2>& words, const char* expectedMessage) {
    bool threw = false;
    try {
        (void)DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
    } catch (const std::runtime_error& e) {
        threw = true;
        EXPECT_EQ(std::string_view(e.what()), std::string_view(expectedMessage));
    }
    EXPECT_EQ(threw, true);
}

void TestDsWriteAddtidTolerance() {
    constexpr std::array<std::uint32_t, 3> data1Values = {0u, 42u, 255u};
    for (const std::uint32_t data1 : data1Values) {
        const std::uint32_t word0 = 0xDAC00000u;
        const std::uint32_t word1 = (data1 << 16) | (5u << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::DS);
        EXPECT_EQ(inst.op, RdnaOpcode::DsWriteAddtidB32);
        EXPECT_EQ(inst.sourceCount, 1u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source1.reg, 5u);
        EXPECT_EQ(inst.source2.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source2.reg, data1);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundLaneId = false;
        bool foundWriteShared = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::LaneId) {
                foundLaneId = true;
            }
            if (val->Opcode() == IrOpcode::WriteSharedU32) {
                foundWriteShared = true;
            }
        }
        EXPECT_EQ(foundLaneId, true);
        EXPECT_EQ(foundWriteShared, true);
    }
}

void TestDsReadAddtidTolerance() {
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> dataPairs = {{
        {0u, 0u},
        {17u, 0u},
        {0u, 88u},
        {255u, 255u}
    }};

    for (const auto& [data0, data1] : dataPairs) {
        const std::uint32_t word0 = 0xDAC40000u;
        const std::uint32_t word1 = (7u << 24) | (data1 << 16) | (data0 << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::DS);
        EXPECT_EQ(inst.op, RdnaOpcode::DsReadAddtidB32);
        EXPECT_EQ(inst.sourceCount, 0u);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.destination.reg, 7u);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundLaneId = false;
        bool foundLoadShared = false;
        bool foundSetDest = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::LaneId) {
                foundLaneId = true;
            }
            if (val->Opcode() == IrOpcode::LoadSharedU32) {
                foundLoadShared = true;
            }
            if (val->Opcode() == IrOpcode::SetVectorRegister && val->Argument(0)->Register().index == 7u) {
                foundSetDest = true;
            }
        }
        EXPECT_EQ(foundLaneId, true);
        EXPECT_EQ(foundLoadShared, true);
        EXPECT_EQ(foundSetDest, true);
    }
}

void Test64BitScalarDescriptorVccLoAndRefusals() {
    {
        const std::uint32_t word0 = 0xDC308000u;
        const std::uint32_t word1 = (1u << 24) | (106u << 16);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::FLAT);
        EXPECT_EQ(inst.op, RdnaOpcode::FlatLoadDword);
        EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source0.reg, 0u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VccLo);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundAddressResource = false;
        bool foundLoadAddress = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::GetAddressResource) {
                foundAddressResource = true;
            }
            if (val->Opcode() == IrOpcode::LoadAddressU32) {
                foundLoadAddress = true;
            }
        }
        EXPECT_EQ(foundAddressResource, true);
        EXPECT_EQ(foundLoadAddress, true);
    }

    {
        const std::uint32_t word0 = 0xDC708000u;
        const std::uint32_t word1 = (106u << 16) | (4u << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::FLAT);
        EXPECT_EQ(inst.op, RdnaOpcode::FlatStoreDword);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.destination.reg, 4u);
        EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source0.reg, 0u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VccLo);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundAddressResource = false;
        bool foundStoreAddress = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::GetAddressResource) {
                foundAddressResource = true;
            }
            if (val->Opcode() == IrOpcode::StoreAddressU32) {
                foundStoreAddress = true;
            }
        }
        EXPECT_EQ(foundAddressResource, true);
        EXPECT_EQ(foundStoreAddress, true);
    }

    ExpectThrows({0xDC588000u, 0x056B0000u}, "global_load_dword_addtid supports only an SGPR pair as base address");
    ExpectThrows({0xDC588000u, 0x007E0000u}, "global_load_dword_addtid supports only an SGPR pair as base address");
    ExpectThrows({0xDC588000u, 0x00690000u}, "global_load_dword_addtid supports only an SGPR pair as base address");

    ExpectThrows({0xDC708000u, 0x007E0201u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDC308000u, 0x046B0001u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDCC88000u, 0x007C0201u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDC308000u, 0x00690001u}, "FLAT scalar address register range overflow");
}

std::array<std::uint32_t, 2> SmemStoreWords(std::uint32_t opcode, std::uint32_t dataReg) {
    const std::uint32_t word0 = (0x3Du << 26u) | (opcode << 18u) | (dataReg << 6u) | 4u;
    return {word0, 0u};
}

void ExpectSmemStoreTranslationThrows(std::uint32_t opcode, RdnaOpcode expected, const char* message) {
    const auto words = SmemStoreWords(opcode, 10u);
    const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
    EXPECT_EQ(inst.family, RdnaInstructionFamily::SMEM);
    EXPECT_EQ(inst.op, expected);
    EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.destination.reg, 10u);
    EXPECT_EQ(inst.source0.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(inst.source0.reg, 8u);
    EXPECT_EQ(inst.sourceCount, 2u);

    bool threw = false;
    try {
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);
    } catch (const std::runtime_error& e) {
        threw = true;
        EXPECT_EQ(std::string_view(e.what()), std::string_view(message));
    }
    EXPECT_EQ(threw, true);
}

void TestSmemStores() {
    constexpr std::array<std::pair<std::uint32_t, RdnaOpcode>, 3> rawStores{{
        {0x10u, RdnaOpcode::SStoreDword},
        {0x11u, RdnaOpcode::SStoreDwordx2},
        {0x12u, RdnaOpcode::SStoreDwordx4},
    }};
    for (const auto& [opcode, expected] : rawStores) {
        const auto words = SmemStoreWords(opcode, 10u);
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::SMEM);
        EXPECT_EQ(inst.op, expected);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
        EXPECT_EQ(inst.destination.reg, 10u);
        EXPECT_EQ(inst.source0.kind, RdnaOperandKind::ScalarRegister);
        EXPECT_EQ(inst.source0.reg, 8u);
        EXPECT_EQ(inst.sourceCount, 2u);
        EXPECT_EQ(inst.dataDwordCount, static_cast<std::uint32_t>(expected == RdnaOpcode::SStoreDword ? 1u : expected == RdnaOpcode::SStoreDwordx2 ? 2u : 4u));

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        std::uint32_t storeCount = 0u;
        std::uint32_t resourceCount = 0u;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::StoreAddressU32) ++storeCount;
            if (val->Opcode() == IrOpcode::GetAddressResource) ++resourceCount;
        }
        EXPECT_EQ(storeCount, static_cast<std::uint32_t>(expected == RdnaOpcode::SStoreDword ? 1u : expected == RdnaOpcode::SStoreDwordx2 ? 2u : 4u));
        EXPECT_EQ(resourceCount, 1u);
    }

    ExpectSmemStoreTranslationThrows(0x15u, RdnaOpcode::SScratchStoreDword, "s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
    ExpectSmemStoreTranslationThrows(0x16u, RdnaOpcode::SScratchStoreDwordx2, "s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
    ExpectSmemStoreTranslationThrows(0x17u, RdnaOpcode::SScratchStoreDwordx4, "s_scratch_store not yet implemented: wave-uniform or swizzled scratch layout required");
    ExpectSmemStoreTranslationThrows(0x18u, RdnaOpcode::SBufferStoreDword, "s_buffer_store not yet implemented: writable scalar buffer descriptor required");
    ExpectSmemStoreTranslationThrows(0x19u, RdnaOpcode::SBufferStoreDwordx2, "s_buffer_store not yet implemented: writable scalar buffer descriptor required");
    ExpectSmemStoreTranslationThrows(0x1au, RdnaOpcode::SBufferStoreDwordx4, "s_buffer_store not yet implemented: writable scalar buffer descriptor required");
}

}

int main() {
    TestDsWriteAddtidTolerance();
    TestDsReadAddtidTolerance();
    Test64BitScalarDescriptorVccLoAndRefusals();
    TestSmemStores();
    return 0;
}

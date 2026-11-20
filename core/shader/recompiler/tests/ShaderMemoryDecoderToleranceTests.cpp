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

void TestScalarStoreDecode() {
    struct Store {
        std::uint32_t word;
        RdnaOpcode opcode;
        std::uint32_t dwords;
    };
    constexpr std::array<Store, 9> stores{{
        {0xf4400104u, RdnaOpcode::SStoreDword, 1u},
        {0xf4440104u, RdnaOpcode::SStoreDwordx2, 2u},
        {0xf4480104u, RdnaOpcode::SStoreDwordx4, 4u},
        {0xf4540104u, RdnaOpcode::SScratchStoreDword, 1u},
        {0xf4580104u, RdnaOpcode::SScratchStoreDwordx2, 2u},
        {0xf45c0104u, RdnaOpcode::SScratchStoreDwordx4, 4u},
        {0xf4600104u, RdnaOpcode::SBufferStoreDword, 1u},
        {0xf4640104u, RdnaOpcode::SBufferStoreDwordx2, 2u},
        {0xf4680104u, RdnaOpcode::SBufferStoreDwordx4, 4u},
    }};
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 5> offsets{{
        {0u, 0u}, {4u, 4u}, {0x0fffffu, 0x0fffffu},
        {0x100000u, 0xfff00000u}, {0x1ffffcu, 0xfffffffcu},
    }};
    for (const auto& store : stores) {
        for (const auto& [encoded, offset] : offsets) {
            for (std::uint32_t flags = 0u; flags < 4u; ++flags) {
                const std::array<std::uint32_t, 4> words{
                    0xbf800000u, store.word | ((flags & 1u) << 16u) | ((flags & 2u) << 13u),
                    0xfa000000u | encoded, 0xbf810000u,
                };
                const auto inst = DecodeRdnaMemoryOp(words, 1u);
                EXPECT_EQ(inst.op, store.opcode);
                EXPECT_EQ(inst.family, RdnaInstructionFamily::SMEM);
                EXPECT_EQ(IsScalarMemoryOpcode(inst.op), true);
                EXPECT_EQ(inst.opcodeId, (store.word >> 18u) & 0xffu);
                EXPECT_EQ(inst.programCounter, 4u);
                EXPECT_EQ(inst.wordCount, 2u);
                EXPECT_EQ(inst.rawWords[0], words[1]);
                EXPECT_EQ(inst.rawWords[1], words[2]);
                EXPECT_EQ(inst.destination.kind, RdnaOperandKind::None);
                EXPECT_EQ(inst.sourceCount, 3u);
                EXPECT_EQ(inst.source0.kind, RdnaOperandKind::ScalarRegister);
                EXPECT_EQ(inst.source0.reg, 8u);
                EXPECT_EQ(inst.source1.kind, RdnaOperandKind::Null);
                EXPECT_EQ(inst.source2.kind, RdnaOperandKind::ScalarRegister);
                EXPECT_EQ(inst.source2.reg, 4u);
                EXPECT_EQ(inst.dataDwordCount, store.dwords);
                EXPECT_EQ(inst.dataBits, 32u);
                EXPECT_EQ(inst.memoryOffset, offset);
                EXPECT_EQ(inst.glc, (flags & 1u) != 0u);
                EXPECT_EQ(inst.dlc, (flags & 2u) != 0u);
            }
        }
        const std::array<std::uint32_t, 3> words{store.word, 0xfa000000u, 0xbf810000u};
        const auto decoded = RdnaInstructionDecoder{}.Decode(words);
        EXPECT_EQ(decoded.instructions.size(), 2u);
        EXPECT_EQ(decoded.instructions[0].op, store.opcode);
        EXPECT_EQ(decoded.instructions[1].op, RdnaOpcode::SEndpgm);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        bool threw = false;
        try {
            context.TranslateInstruction(decoded.instructions[0]);
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string_view(error.what()), std::string_view("opcode has no IR translation at pc 0"));
            threw = true;
        }
        Require(threw, "scalar store must not translate as a no-op");
    }

    const std::array<std::uint32_t, 2> registerOffset{0xf4454104u, 0x141ffffcu};
    const auto offset = DecodeRdnaMemoryOp(registerOffset, 0u);
    EXPECT_EQ(offset.op, RdnaOpcode::SStoreDwordx2);
    EXPECT_EQ(offset.source1.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(offset.source1.reg, 10u);
    EXPECT_EQ(offset.memoryOffset, 0xfffffffcu);
    EXPECT_EQ(offset.glc, true);
    EXPECT_EQ(offset.dlc, true);

    const std::array<std::uint32_t, 2> lastScalar{0xf4401a74u, 0xd2000000u};
    const auto last = DecodeRdnaMemoryOp(lastScalar, 0u);
    EXPECT_EQ(last.source0.reg, 104u);
    EXPECT_EQ(last.source1.reg, 105u);
    EXPECT_EQ(last.source2.reg, 105u);

    const std::array<std::uint32_t, 2> vcc{0xf4441ab5u, 0xfa000000u};
    const auto special = DecodeRdnaMemoryOp(vcc, 0u);
    EXPECT_EQ(special.source0.kind, RdnaOperandKind::VccLo);
    EXPECT_EQ(special.source2.kind, RdnaOperandKind::VccLo);

    for (const auto& store : stores) {
        const std::array<std::uint32_t, 2> words{(store.word & ~0x1fc0u) | (125u << 6u), 0xfa000000u};
        EXPECT_EQ(DecodeRdnaMemoryOp(words, 0u).source2.kind, RdnaOperandKind::Null);
    }
}

void TestScalarStoreRefusals() {
    ExpectThrows({0xf4440000u | (103u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4480000u | (102u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4480000u | (104u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4400000u | (124u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4400000u | (126u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4440000u | (107u << 6u), 0xfa000000u}, "unsupported SMEM store data register range");
    ExpectThrows({0xf4600101u, 0xfa000000u}, "SMEM store buffer descriptor requires four aligned SGPRs");
    ExpectThrows({0xf4600134u, 0xfa000000u}, "SMEM store buffer descriptor requires four aligned SGPRs");
    ExpectThrows({0xf4600135u, 0xfa000000u}, "SMEM store buffer descriptor requires four aligned SGPRs");
    ExpectThrows({0xf4400100u, 108u << 25u}, "unsupported scalar source operand");
    ExpectThrows({0xf44c0104u, 0xfa000000u}, "SMEM opcode is not supported");
}

}

int main() {
    TestDsWriteAddtidTolerance();
    TestDsReadAddtidTolerance();
    Test64BitScalarDescriptorVccLoAndRefusals();
    TestScalarStoreDecode();
    TestScalarStoreRefusals();
    return 0;
}

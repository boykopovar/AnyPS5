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

}

void TestGfx1013OnlyFormsDecode() {
    struct FormCase {
        RdnaOpcode op;
        std::uint32_t opcode;
        std::uint32_t dwords;
        bool ds;
    };
    static const FormCase forms[] = {
        {RdnaOpcode::DsAddSrc2U64, 0xc0u, 2u, true},
        {RdnaOpcode::DsAndSrc2B64, 0xc9u, 2u, true},
        {RdnaOpcode::DsDecSrc2U64, 0xc4u, 2u, true},
        {RdnaOpcode::DsIncSrc2U64, 0xc3u, 2u, true},
        {RdnaOpcode::DsMaxSrc2F64, 0xd3u, 2u, true},
        {RdnaOpcode::DsMaxSrc2I64, 0xc6u, 2u, true},
        {RdnaOpcode::DsMaxSrc2U64, 0xc8u, 2u, true},
        {RdnaOpcode::DsMinSrc2F64, 0xd2u, 2u, true},
        {RdnaOpcode::DsMinSrc2I64, 0xc5u, 2u, true},
        {RdnaOpcode::DsMinSrc2U64, 0xc7u, 2u, true},
        {RdnaOpcode::DsOrSrc2B64, 0xcau, 2u, true},
        {RdnaOpcode::DsRsubSrc2U64, 0xc2u, 2u, true},
        {RdnaOpcode::DsSubSrc2U64, 0xc1u, 2u, true},
        {RdnaOpcode::DsWriteSrc2B64, 0xcdu, 2u, true},
        {RdnaOpcode::DsXorSrc2B64, 0xcbu, 2u, true},
        {RdnaOpcode::SAtomicAdd, 0x82u, 1u, false},
        {RdnaOpcode::SAtomicAddX2, 0xa2u, 2u, false},
        {RdnaOpcode::SAtomicAnd, 0x88u, 1u, false},
        {RdnaOpcode::SAtomicAndX2, 0xa8u, 2u, false},
        {RdnaOpcode::SAtomicCmpswap, 0x81u, 1u, false},
        {RdnaOpcode::SAtomicCmpswapX2, 0xa1u, 2u, false},
        {RdnaOpcode::SAtomicDec, 0x8cu, 1u, false},
        {RdnaOpcode::SAtomicDecX2, 0xacu, 2u, false},
        {RdnaOpcode::SAtomicInc, 0x8bu, 1u, false},
        {RdnaOpcode::SAtomicIncX2, 0xabu, 2u, false},
        {RdnaOpcode::SAtomicOr, 0x89u, 1u, false},
        {RdnaOpcode::SAtomicOrX2, 0xa9u, 2u, false},
        {RdnaOpcode::SAtomicSmax, 0x86u, 1u, false},
        {RdnaOpcode::SAtomicSmaxX2, 0xa6u, 2u, false},
        {RdnaOpcode::SAtomicSmin, 0x84u, 1u, false},
        {RdnaOpcode::SAtomicSminX2, 0xa4u, 2u, false},
        {RdnaOpcode::SAtomicSub, 0x83u, 1u, false},
        {RdnaOpcode::SAtomicSubX2, 0xa3u, 2u, false},
        {RdnaOpcode::SAtomicSwap, 0x80u, 1u, false},
        {RdnaOpcode::SAtomicSwapX2, 0xa0u, 2u, false},
        {RdnaOpcode::SAtomicUmax, 0x87u, 1u, false},
        {RdnaOpcode::SAtomicUmaxX2, 0xa7u, 2u, false},
        {RdnaOpcode::SAtomicUmin, 0x85u, 1u, false},
        {RdnaOpcode::SAtomicUminX2, 0xa5u, 2u, false},
        {RdnaOpcode::SAtomicXor, 0x8au, 1u, false},
        {RdnaOpcode::SAtomicXorX2, 0xaau, 2u, false},
        {RdnaOpcode::SBufferAtomicAdd, 0x42u, 1u, false},
        {RdnaOpcode::SBufferAtomicAddX2, 0x62u, 2u, false},
        {RdnaOpcode::SBufferAtomicAnd, 0x48u, 1u, false},
        {RdnaOpcode::SBufferAtomicAndX2, 0x68u, 2u, false},
        {RdnaOpcode::SBufferAtomicCmpswap, 0x41u, 1u, false},
        {RdnaOpcode::SBufferAtomicCmpswapX2, 0x61u, 2u, false},
        {RdnaOpcode::SBufferAtomicDec, 0x4cu, 1u, false},
        {RdnaOpcode::SBufferAtomicDecX2, 0x6cu, 2u, false},
        {RdnaOpcode::SBufferAtomicInc, 0x4bu, 1u, false},
        {RdnaOpcode::SBufferAtomicIncX2, 0x6bu, 2u, false},
        {RdnaOpcode::SBufferAtomicOr, 0x49u, 1u, false},
        {RdnaOpcode::SBufferAtomicOrX2, 0x69u, 2u, false},
        {RdnaOpcode::SBufferAtomicSmax, 0x46u, 1u, false},
        {RdnaOpcode::SBufferAtomicSmaxX2, 0x66u, 2u, false},
        {RdnaOpcode::SBufferAtomicSmin, 0x44u, 1u, false},
        {RdnaOpcode::SBufferAtomicSminX2, 0x64u, 2u, false},
        {RdnaOpcode::SBufferAtomicSub, 0x43u, 1u, false},
        {RdnaOpcode::SBufferAtomicSubX2, 0x63u, 2u, false},
        {RdnaOpcode::SBufferAtomicSwap, 0x40u, 1u, false},
        {RdnaOpcode::SBufferAtomicSwapX2, 0x60u, 2u, false},
        {RdnaOpcode::SBufferAtomicUmax, 0x47u, 1u, false},
        {RdnaOpcode::SBufferAtomicUmaxX2, 0x67u, 2u, false},
        {RdnaOpcode::SBufferAtomicUmin, 0x45u, 1u, false},
        {RdnaOpcode::SBufferAtomicUminX2, 0x65u, 2u, false},
        {RdnaOpcode::SBufferAtomicXor, 0x4au, 1u, false},
        {RdnaOpcode::SBufferAtomicXorX2, 0x6au, 2u, false},
        {RdnaOpcode::SBufferStoreDword, 0x18u, 1u, false},
        {RdnaOpcode::SBufferStoreDwordx2, 0x19u, 2u, false},
        {RdnaOpcode::SBufferStoreDwordx4, 0x1au, 4u, false},
        {RdnaOpcode::SScratchStoreDword, 0x15u, 1u, false},
        {RdnaOpcode::SScratchStoreDwordx2, 0x16u, 2u, false},
        {RdnaOpcode::SScratchStoreDwordx4, 0x17u, 4u, false},
        {RdnaOpcode::SStoreDword, 0x10u, 1u, false},
        {RdnaOpcode::SStoreDwordx2, 0x11u, 2u, false},
        {RdnaOpcode::SStoreDwordx4, 0x12u, 4u, false},
    };
    for (const FormCase& form : forms) {
        const std::uint32_t word0 = form.ds
            ? ((0x36u << 26u) | (form.opcode << 18u) | 2u)
            : ((0x3Du << 26u) | (form.opcode << 18u) | (3u << 6u) | 2u);
        const std::uint32_t word1 = form.ds ? ((4u << 24u) | (6u << 16u) | (5u << 8u) | 7u) : 0u;
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
        EXPECT_EQ(inst.op, form.op);
        EXPECT_EQ(inst.dataDwordCount, form.dwords);
        EXPECT_EQ(IsScalarMemoryOpcode(inst.op), !form.ds);
    }

    const std::array<std::uint32_t, 2> dsWords = {(0x36u << 26u) | (0xC0u << 18u) | 2u, (4u << 24u) | (6u << 16u) | (5u << 8u) | 7u};
    const RdnaInstruction ds = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(dsWords), 0u);
    EXPECT_EQ(ds.secondaryOffset, 8u);
    EXPECT_EQ(IsScalarMemoryOpcode(RdnaOpcode::SMovB32), false);
}


void TestGfx1013OnlyFormsTranslate() {
    const std::array<std::uint32_t, 2> storeWords = {(0x3Du << 26u) | (0x10u << 18u) | (3u << 6u) | 2u, 0x40u};
    const RdnaInstruction store = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(storeWords), 0u);
    {
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(store);
        bool foundStore = false;
        for (const auto* value : block.Instructions())
            if (value->Opcode() == IrOpcode::StoreAddressU32) foundStore = true;
        EXPECT_EQ(foundStore, true);
    }

    const std::array<std::uint32_t, 2> bufferStoreWords = {(0x3Du << 26u) | (0x18u << 18u) | (3u << 6u) | 2u, 0x40u};
    const RdnaInstruction bufferStore = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(bufferStoreWords), 0u);
    {
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(bufferStore);
        bool foundStore = false;
        for (const auto* value : block.Instructions())
            if (value->Opcode() == IrOpcode::StoreBufferU32) foundStore = true;
        EXPECT_EQ(foundStore, true);
    }

    const std::array<std::uint32_t, 2> src2Words = {(0x36u << 26u) | (0xC0u << 18u) | 2u, (4u << 24u) | (6u << 16u) | (5u << 8u) | 7u};
    const RdnaInstruction src2 = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(src2Words), 0u);
    {
        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(src2);
        bool foundLoad = false;
        bool foundAtomic = false;
        for (const auto* value : block.Instructions()) {
            if (value->Opcode() == IrOpcode::LoadSharedU32x2) foundLoad = true;
            if (value->Opcode() == IrOpcode::SharedAtomicIAdd64) foundAtomic = true;
        }
        EXPECT_EQ(foundLoad, true);
        EXPECT_EQ(foundAtomic, true);
    }
}

int main() {
    TestDsWriteAddtidTolerance();
    TestDsReadAddtidTolerance();
    Test64BitScalarDescriptorVccLoAndRefusals();
    TestGfx1013OnlyFormsDecode();
    TestGfx1013OnlyFormsTranslate();
    return 0;
}

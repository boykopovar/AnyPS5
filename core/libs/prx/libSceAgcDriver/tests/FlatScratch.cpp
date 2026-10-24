#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace ShaderRecompiler;
static unsigned tests = 0;
#define Require(value)                                                         \
  do {                                                                         \
    if (!(value)) {                                                            \
      std::fprintf(stderr, "flat scratch assertion line %u: %s\n", __LINE__,   \
                   #value);                                                    \
      throw std::runtime_error("flat scratch regression");                     \
    }                                                                          \
  } while (false)
static RdnaInstruction Decode(unsigned seg, unsigned saddr, unsigned addr,
                              unsigned offset = 0, unsigned opcode = 12) {
  const std::array<unsigned, 2> code{(0x37u << 26) | (opcode << 18) |
                                         (seg << 14) | offset,
                                     (2u << 24) | (saddr << 16) | addr};
  return DecodeRdnaFlat(0x154cu, code, 0);
}
static void Reject(unsigned seg, unsigned saddr, unsigned addr) {
  try {
    (void)Decode(seg, saddr, addr);
  } catch (const std::runtime_error &) {
    ++tests;
    return;
  }
  throw std::runtime_error("invalid FLAT accepted");
}
static void Scratch(unsigned saddr, unsigned addr, unsigned expectedVector,
                    unsigned expectedScalar) {
  auto op = Decode(1, saddr, addr, 0xffcu);
  Require(op.memorySegment == 1 && op.memoryOffset == -4 &&
          op.sourceCount == 2);
  Require(op.source0.kind == (expectedVector
                                  ? RdnaOperandKind::VectorRegister
                                  : RdnaOperandKind::IntegerInlineConstant));
  Require(op.source1.kind == (expectedScalar
                                  ? RdnaOperandKind::ScalarRegister
                                  : RdnaOperandKind::IntegerInlineConstant));
  if (expectedVector)
    Require(op.source0.reg == addr);
  else
    Require(op.source0.value == 0);
  if (expectedScalar)
    Require(op.source1.reg == saddr);
  else
    Require(op.source1.value == 0);
  IrProgram program;
  auto &block = program.CreateBlock();
  program.SetEntryBlock(block);
  TranslationContext context(program, block, 256);
  context.TranslateInstruction(op);
  unsigned vectors = 0, addressReads = 0, destinationReads = 0, scalars = 0,
           loads = 0, scratch = 0;
  for (auto *inst : block.Instructions()) {
    if (inst->Opcode() == IrOpcode::GetVectorRegister) {
      ++vectors;
      auto reg = inst->Arguments().at(0)->Register().index;
      Require(reg < 256);
      if (reg == addr)
        ++addressReads;
      if (reg == 2)
        ++destinationReads;
    }
    if (inst->Opcode() == IrOpcode::GetScalarRegister)
      ++scalars;
    if (inst->Opcode() == IrOpcode::LoadAddressU32)
      ++loads;
    if (inst->Opcode() == IrOpcode::GetScratchResource)
      ++scratch;
  }
  std::printf(
      "SV/SS/ST saddr=%u addr=%u vector=%u scalar=%u loads=%u scratch=%u\n",
      saddr, addr, vectors, scalars, loads, scratch);
  std::fflush(stdout);
  Require(vectors == expectedVector + 1 && addressReads == expectedVector &&
          destinationReads == 1 && scalars == expectedScalar && loads == 1 &&
          scratch == 1);
  ++tests;
}
int main() {

  const std::array<unsigned, 2> code{0xdc304000u, 0x027d00ffu};
  auto actual = DecodeRdnaFlat(0x154cu, code, 0);
  Require(actual.destination.reg == 2 && actual.source0.reg == 255 &&
          actual.source1.value == 0);
  ++tests;
  Scratch(125, 255, 1, 0);
  Scratch(125, 254, 1, 0);
  Scratch(0, 255, 0, 1);
  Scratch(105, 255, 0, 1);
  Scratch(127, 255, 0, 0);
  Reject(0, 125, 255);
  Reject(2, 125, 255);
  Reject(2, 127, 255);
  Reject(1, 108, 0);
  Reject(3, 0, 0);
  auto flat = Decode(0, 125, 254);
  Require(flat.source1.kind == RdnaOperandKind::VectorRegister &&
          flat.source1.reg == 255);
  ++tests;
  auto global = Decode(2, 0, 255);
  Require(global.source0.reg == 255 &&
          global.source1.kind == RdnaOperandKind::ScalarRegister);
  ++tests;
  try {
    const std::array<unsigned, 1> truncated{0xdc304000u};
    (void)DecodeRdnaFlat(0, truncated, 0);
    throw std::logic_error("truncated accepted");
  } catch (const std::out_of_range &) {
    ++tests;
  }
  std::printf("PASS %u flat scratch decode/IR and invalid boundary cases\n",
              tests);
}

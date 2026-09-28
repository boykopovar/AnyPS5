#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;
static void Require(bool value) { if (!value) throw std::runtime_error("buffer address operand regression"); }
static void CheckRegister(IrValue* value, IrOpcode opcode, unsigned index) {
    Require(value->Opcode() == opcode);
    Require(value->Argument(0)->Register().index == index);
}
static void Check(bool typed, unsigned opcode, IrOpcode expected, bool idxen, bool offen, bool immediate) {
    const std::array<std::uint32_t,2> code{
        (typed ? (0x3au<<26)|(opcode<<16)|(4u<<19) : (0x38u<<26)|(opcode<<18))
            | (unsigned(idxen)<<13)|(unsigned(offen)<<12)|12u,
        ((immediate?128u:20u)<<24)|(2u<<16)|(4u<<8)|10u
    };
    const auto instruction = typed ? DecodeRdnaMtbuf(0,code,0) : DecodeRdnaMubuf(0,code,0);
    IrProgram program;
    auto& block=program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program,block,256);
    context.TranslateInstruction(instruction);
    IrValue* memory=nullptr;
    for(auto* value:block.Instructions()) if(value->Opcode()==expected) memory=value;
    Require(memory!=nullptr);
    if(idxen) CheckRegister(memory->Argument(1),IrOpcode::GetVectorRegister,10);
    else Require(memory->Argument(1)->HasImmediate() && memory->Argument(1)->ImmediateU32()==0);
    if(offen) CheckRegister(memory->Argument(2),IrOpcode::GetVectorRegister,idxen?11:10);
    else Require(memory->Argument(2)->HasImmediate() && memory->Argument(2)->ImmediateU32()==0);
    if(immediate) Require(memory->Argument(3)->HasImmediate() && memory->Argument(3)->ImmediateU32()==0);
    else CheckRegister(memory->Argument(3),IrOpcode::GetScalarRegister,20);
}
int main() {
    for(bool idxen:{false,true}) for(bool offen:{false,true}) for(bool immediate:{false,true}) {
        Check(false,0x0c,IrOpcode::LoadBufferU32,idxen,offen,immediate);
        Check(false,0x1c,IrOpcode::StoreBufferU32,idxen,offen,immediate);
        Check(false,0x32,IrOpcode::BufferAtomicIAdd32,idxen,offen,immediate);
        Check(true,0,IrOpcode::LoadBufferU32,idxen,offen,immediate);
        Check(true,4,IrOpcode::StoreBufferU32,idxen,offen,immediate);
    }
}

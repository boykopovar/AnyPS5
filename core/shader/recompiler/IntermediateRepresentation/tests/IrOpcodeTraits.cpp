#include "IntermediateRepresentation/IrOpcode.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using namespace ShaderRecompiler;

} // namespace

int main() {
    Require(BufferAccessOf(IrOpcode::LoadBufferU32) == BufferAccess::Read);
    Require(BufferAccessOf(IrOpcode::ReadConstBuffer) == BufferAccess::Read);
    Require(BufferAccessOf(IrOpcode::StoreBufferU32) == BufferAccess::Write);
    Require(BufferAccessOf(IrOpcode::BufferAtomicSwap32) == BufferAccess::Atomic);
    Require(BufferAccessOf(IrOpcode::IAdd32) == BufferAccess::None);

    Require(SharedAccessOf(IrOpcode::LoadSharedU32) == SharedAccess::Read);
    Require(SharedAccessOf(IrOpcode::StoreShared) == SharedAccess::Write);
    Require(SharedAccessOf(IrOpcode::IAdd32) == SharedAccess::None);

    Require(AddressOpcodeInfoOf(IrOpcode::IAdd32).access == AddressAccess::None);
    Require(ImageOpcodeInfoOf(IrOpcode::IAdd32).access == ImageAccess::None);

    Require(IrOpcodeHasSideEffects(IrOpcode::SetRegister));
    Require(IrOpcodeHasSideEffects(IrOpcode::Discard));
    Require(!IrOpcodeHasSideEffects(IrOpcode::IAdd32));

    return 0;
}

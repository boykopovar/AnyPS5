#include "BdaAbi.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "Optimization/ConstantFolder.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace ShaderRecompiler;

namespace {

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    IrProgram program;
    IrBuilder builder{program};
    IrValue* a = nullptr;
    IrValue* b = nullptr;

    Fixture() {
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        builder.SetInsertionPoint(block);
        a = &builder.Emit(IrOpcode::LaneId, IrType::U32, {});
        b = &builder.Emit(IrOpcode::IAdd32, IrType::U32, {&builder.Constant(1u), a});
    }

    IrValue& Xor(IrValue& lhs, IrValue& rhs) {
        return builder.Emit(IrOpcode::BitwiseXor32, IrType::U32, {&lhs, &rhs});
    }

    IrValue& Use(IrValue& value) {
        return builder.Emit(IrOpcode::ReferenceU32, IrType::Void, {&value});
    }

    void Fold() {
        static_cast<void>(builder.Emit(IrOpcode::Return, IrType::Void, {}));
        ConstantFolder().Fold(program);
    }
};

const IrValue* Folded(const IrValue& user) {
    return user.Argument(0)->Resolve();
}

void CheckSwap() {
    Fixture f;
    auto& first = f.Xor(*f.a, *f.b);
    auto& second = f.Xor(first, *f.b);
    auto& third = f.Xor(first, second);
    auto& swappedB = f.Use(second);
    auto& swappedA = f.Use(third);
    f.Fold();
    Check(Folded(swappedB) == f.a, "(a ^ b) ^ b did not fold to a");
    Check(Folded(swappedA) == f.b, "(a ^ b) ^ a did not fold to b");
}

void CheckOperandOrders() {
    for (std::uint32_t order = 0; order < 4u; ++order) {
        Fixture f;
        auto& inner = (order & 1u) != 0u ? f.Xor(*f.b, *f.a) : f.Xor(*f.a, *f.b);
        auto& outer = (order & 2u) != 0u ? f.Xor(*f.b, inner) : f.Xor(inner, *f.b);
        auto& user = f.Use(outer);
        f.Fold();
        Check(Folded(user) == f.a, "operand order " + std::to_string(order) + " of (a ^ b) ^ b did not fold to a");
    }
}

void CheckSelfAndUnrelated() {
    Fixture f;
    auto& self = f.Use(f.Xor(*f.a, *f.a));
    auto& c = f.builder.Emit(IrOpcode::IAdd32, IrType::U32, {&f.builder.Constant(2u), f.a});
    auto& unrelated = f.Xor(f.Xor(*f.a, *f.b), c);
    auto& kept = f.Use(unrelated);
    f.Fold();
    Check(Folded(self)->HasImmediate() && Folded(self)->ImmediateU32() == 0u, "a ^ a did not fold to 0");
    Check(Folded(kept) == &unrelated, "(a ^ b) ^ c changed");
}

SpirvTarget Target() {
    static constexpr std::array<std::uint32_t, 5> capabilities{
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance, spv::CapabilityInt64Atomics,
    };
    static constexpr std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer",
    };
    return {0x00401000u, 0x00010300u, 32, BdaAbi::Version, capabilities, extensions, false,
            {1024, 1024, 64}, 1024, 32768, {}, {}};
}

void CheckSwappedTablePointer() {
    alignas(256) static constexpr std::array<std::uint32_t, 20> Code{
        0x7e060500u, 0xbe940300u, 0xbe950301u, 0xbf068003u, 0xbf850005u, 0xbe9e0301u, 0xbe950303u, 0x891e151eu, 0x8915151eu, 0x891e151eu,
        0xf40c020au, 0xfa000000u, 0x7e020300u, 0x7e040300u, 0x7e060300u, 0x7e080300u, 0xbf8cc07fu, 0xf0201f00u, 0x00020100u, 0xbf810000u,
    };
    constexpr std::uint32_t Width = 32u;
    constexpr std::uint32_t Format32x4Float = 77u;
    constexpr std::uint32_t Type1D = 8u;
    alignas(256) static std::array<std::uint32_t, Width * 4u> texels{};
    alignas(256) static std::array<std::uint32_t, 8> table{};
    const auto texelAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels.data()));
    table = {
        static_cast<std::uint32_t>(texelAddress >> 8u),
        static_cast<std::uint32_t>((texelAddress >> 40u) & 0xffu) | (Format32x4Float << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u,
        0xfacu | (Type1D << 28u),
        0u, 0u, 0u, 0u,
    };
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(table.data());
    const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u};
    const std::span<const std::uint32_t> words(Code);
    const std::array<MemoryRegion, 3> memory{{
        {reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(words)},
        {tableAddress, std::as_bytes(std::span<const std::uint32_t>(table))},
        {static_cast<std::uintptr_t>(texelAddress), std::as_bytes(std::span<const std::uint32_t>(texels))},
    }};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), words, 0, {}};
    request.target = Target();
    request.layout = {0, 0, 0, 128};
    request.context.waveSize = 32;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.context.memory = memory;
    request.useCache = false;
    const auto result = Recompile(request);
    Check(!result.bindings.empty(), "a T# loaded through an XOR-swapped table pointer produced no binding");
}

}

int main() {
    try {
        CheckSwap();
        CheckOperandOrders();
        CheckSelfAndUnrelated();
        CheckSwappedTablePointer();
        std::puts("XOR cancellation and XOR-swapped descriptor pointers pass");
        return 0;
    } catch (const std::exception& error) {
        const std::string message = error.what();
        std::fprintf(stderr, "%s\n", message.substr(0, message.find("\nRecompileRequest:")).c_str());
        return 1;
    }
}

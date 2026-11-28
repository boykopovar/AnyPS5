#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckRange(std::uint64_t bufferAddress, std::uint32_t stride, std::uint32_t records, std::uint64_t tableAddress, bool allowed) {
    using namespace ShaderRecompiler;
    using namespace AgcDriver::DriverDetail;
    ShaderBufferResource raw;
    raw.UpdateAddress48(bufferAddress);
    raw.fields[1] |= stride << 16u;
    raw.fields[2] = records;
    raw.fields[3] = 0x31016facu;
    auto compiled = std::make_shared<RecompileResult>();
    DescriptorBinding buffers;
    buffers.role = DescriptorRole::GuestBuffers;
    buffers.guestDescriptor.assign(raw.fields.begin(), raw.fields.end());
    compiled->bindings.push_back(buffers);
    auto flat = buffers;
    flat.role = DescriptorRole::FlattenedSrt;
    compiled->bindings.push_back(flat);
    compiled->bufferBaseSlots.push_back({0u, 0u, 0u});
    DispatchVariant variant;
    variant.compiled = compiled;
    variant.runs.emplace_back(tableAddress, tableAddress + 16u);
    variant.words.assign(raw.fields.begin(), raw.fields.end());
    const std::array words{BufferBaseWord{0u, tableAddress, 0, raw.Base48(), raw.GetSize(), true}};
    TagDrawBufferBases_nid_no_patch(variant, words, true);
    Check(variant.dataPositions.empty() != allowed, "captured buffer range was misclassified for base relocation");
    if (allowed) {
        Check(variant.dataPositions.size() == 1u && variant.dataPositions[0] == 0u, "base relocation did not select only the base word");
        Check(variant.bufferWordBuffers.size() == 1u && variant.bufferWordBuffers[0].size() == 2u, "base relocation did not patch both the buffer and flat SRT copy");
    }
    DispatchVariant unknown;
    unknown.compiled = compiled;
    unknown.runs = variant.runs;
    unknown.words = variant.words;
    const std::array missingRange{BufferBaseWord{0u, tableAddress, 0}};
    TagDrawBufferBases_nid_no_patch(unknown, missingRange, true);
    Check(unknown.dataPositions.empty(), "buffer relocation accepted an unknown buffer range");
}

}

int main() {
    try {
        constexpr std::uint64_t base = 0x100004000ull;
        CheckRange(base, 0u, 64u, base, false);
        CheckRange(base, 0u, 64u, base + 32u, false);
        CheckRange(base, 0u, 64u, base - 15u, false);
        CheckRange(base, 0u, 64u, base + 64u, true);
        CheckRange(base, 0u, 64u, base - 16u, true);
        CheckRange(base, 16u, 4u, base + 32u, false);
        CheckRange(base, 16u, 4u, base + 64u, true);
        CheckRange(base, 16u, 4u, base - 16u, true);
        CheckRange(base, 0u, 64u, 0x200004000ull, true);
        CheckRange(base, 0x3fffu, 0xffffffffu, base + 0x100000000ull, false);
        std::cout << "Buffer base tagging preserves captured byte/strided buffers and 48-bit ranges\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "GuestRuntime.hpp"
#include <limits>
#include <stdexcept>
namespace anyps5::experimental {
GuestRuntime::GuestRuntime(std::vector<std::uint8_t> code, std::size_t memorySize,
                           std::size_t dataSize, std::size_t stackBase)
    : cpu_(code, memorySize) {
    cpu_.setMemoryRegions(dataBase, dataBase + dataSize, stackBase);
}
GuestRuntime GuestRuntime::load(std::span<const std::uint8_t> image,
                                std::size_t stackSize) {
    if (image.size() < 20 || image[0] != 'A' || image[1] != 'G' ||
        image[2] != '6' || image[3] != '4')
        throw std::invalid_argument("Invalid AG64 image magic or header");
    const auto read32 = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(image[offset]) |
            (static_cast<std::uint32_t>(image[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(image[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(image[offset + 3]) << 24);
    };
    const auto version = read32(4);
    const auto codeSize = static_cast<std::size_t>(read32(8));
    const auto dataSize = static_cast<std::size_t>(read32(12));
    const auto entry = static_cast<std::size_t>(read32(16));
    const auto headerSize = version == 2 ? std::size_t{24} : std::size_t{20};
    if ((version != 1 && version != 2) || image.size() < headerSize)
        throw std::invalid_argument("Unsupported AG64 image version");
    const auto relocCount = version == 2 ? static_cast<std::size_t>(read32(20)) : 0;
    if (!codeSize || entry >= codeSize ||
        codeSize > image.size() - headerSize ||
        dataSize > image.size() - headerSize - codeSize ||
        relocCount > (image.size() - headerSize - codeSize - dataSize) / 8 ||
        image.size() != headerSize + codeSize + dataSize + relocCount * 8)
        throw std::invalid_argument("Invalid AG64 image sections or entry");
    if (codeSize > (1u << 20) || dataSize == 0 || dataSize % 4 != 0 ||
        dataSize > (1u << 20) || stackSize < 128 ||
        stackSize > (1u << 20))
        throw std::invalid_argument("Invalid AG64 data/stack size");
    const auto stackBase = static_cast<std::size_t>(dataBase) + dataSize + 4096;
    const auto memorySize = stackBase + stackSize;
    // A five-byte entry trampoline preserves existing CPU IP semantics.
    std::vector<std::uint8_t> code{0xE9};
    const auto displacement = static_cast<std::uint32_t>(entry);
    for (unsigned i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(displacement >> (8 * i)));
    code.insert(code.end(), image.begin() + headerSize,
                image.begin() + headerSize + codeSize);
    // AG64 v2 relocation: {u32 codeOffset, u32 targetDataOffset}.
    // Patch a 32-bit absolute guest-data pointer in the code section.
    std::vector<bool> patched(codeSize, false);
    for (std::size_t i = 0; i < relocCount; ++i) {
        const auto record = headerSize + codeSize + dataSize + i * 8;
        const auto codeOffset = static_cast<std::size_t>(read32(record));
        const auto dataOffset = static_cast<std::size_t>(read32(record + 4));
        if (codeOffset > codeSize || codeSize - codeOffset < 4 ||
            dataOffset >= dataSize)
            throw std::invalid_argument("AG64 relocation out of bounds");
        for (std::size_t byte = 0; byte < 4; ++byte) {
            if (patched[codeOffset + byte])
                throw std::invalid_argument("Overlapping AG64 relocations");
            patched[codeOffset + byte] = true;
        }
        const auto target = static_cast<std::uint32_t>(dataBase + dataOffset);
        for (unsigned byte = 0; byte < 4; ++byte)
            code[5 + codeOffset + byte] = static_cast<std::uint8_t>(target >> (8 * byte));
    }
    GuestRuntime runtime(std::move(code), memorySize, dataSize, stackBase);
    // Initialize only mapped data; the guard page is intentionally unmapped.
    for (std::size_t i = 0; i < dataSize; i += 4) {
        std::uint32_t word = 0;
        for (std::size_t j = 0; j < 4; ++j)
            word |= static_cast<std::uint32_t>(image[headerSize + codeSize + i + j]) << (8 * j);
        runtime.cpu_.writeMemory32(dataBase + i, word);
    }
    runtime.cpu_.setRegister(4, memorySize);
    return runtime;
}
std::uint64_t GuestRuntime::run(std::size_t instructionLimit) {
    return cpu_.run(instructionLimit);
}
}

#include "prx/libSceAjmi/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address) {
    const std::size_t size = (data_input_size + 7) & ~std::size_t{7};
    auto* at = static_cast<std::uint8_t*>(position);
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & ~0x1Fu) | 7u;
    words[1] = static_cast<std::uint32_t>(size);
    std::memcpy(at, words, sizeof(words));
    at += sizeof(words);
    if (data_input_size) std::memcpy(at, data_input, data_input_size);
    *batch_address = at;
    return at + size;
}

static std::uint8_t* KeepChunk(std::uint8_t* at, std::uint32_t keep, std::uint32_t low, std::uint32_t word) {
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & keep) | low;
    words[1] = word;
    std::memcpy(at, words, sizeof(words));
    return at + sizeof(words);
}

static std::uint8_t* KeepBuffer(std::uint8_t* at, std::uint32_t kind, const void* address, std::size_t size) {
    at = KeepChunk(at, 0xFFFFFFE0u, kind, static_cast<std::uint32_t>(size));
    std::memcpy(at, &address, sizeof(address));
    return at + sizeof(address);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    auto* job = static_cast<std::uint8_t*>(position);
    auto* at = job + 2 * sizeof(std::uint32_t);
    if (return_address) at = KeepBuffer(at, 6, return_address, 0);
    at = KeepBuffer(at, 1, data_input, data_input_size);
    const std::uint64_t kept = flags & 0x0000E00000001FFF;
    at = KeepChunk(at, 0xFC000000u, 4 | ((static_cast<std::uint32_t>(kept >> 32) & 0xFFFFFu) << 6), static_cast<std::uint32_t>(kept));
    at = KeepBuffer(at, 0x11, data_output, data_output_size);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    KeepChunk(job, 0xFC000000u, (instance & 0xFFFFFu) << 6, static_cast<std::uint32_t>(at - job) - 2 * sizeof(std::uint32_t));
    return at;
}

}

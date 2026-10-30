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

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    auto* job = static_cast<std::uint8_t*>(position);
    auto* at = job + 2 * sizeof(std::uint32_t);
    if (return_address) {
        const std::uint32_t words[2] = {6u, 0u};
        std::memcpy(at, words, sizeof(words));
        at += sizeof(words);
        std::memcpy(at, &return_address, sizeof(return_address));
        at += sizeof(return_address);
    }
    {
        const std::uint32_t words[2] = {1u, static_cast<std::uint32_t>(data_input_size)};
        std::memcpy(at, words, sizeof(words));
        at += sizeof(words);
        std::memcpy(at, &data_input, sizeof(data_input));
        at += sizeof(data_input);
    }
    {
        const std::uint32_t words[2] = {4u | ((static_cast<std::uint32_t>(flags >> 32) & 0xFFFFFu) << 6), static_cast<std::uint32_t>(flags)};
        std::memcpy(at, words, sizeof(words));
        at += sizeof(words);
    }
    {
        const std::uint32_t words[2] = {17u, static_cast<std::uint32_t>(data_output_size)};
        std::memcpy(at, words, sizeof(words));
        at += sizeof(words);
        std::memcpy(at, &data_output, sizeof(data_output));
        at += sizeof(data_output);
    }
    {
        const std::uint32_t words[2] = {18u, static_cast<std::uint32_t>(sideband_output_size)};
        std::memcpy(at, words, sizeof(words));
        at += sizeof(words);
        std::memcpy(at, &sideband_output, sizeof(sideband_output));
        at += sizeof(sideband_output);
    }
    const std::uint32_t jobWords[2] = {((instance & 0xFFFFFu) << 6), static_cast<std::uint32_t>(at - job - 2 * sizeof(std::uint32_t))};
    std::memcpy(job, jobWords, sizeof(jobWords));
    return at;
}

}

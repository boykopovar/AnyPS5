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

static std::uint8_t* KeepFlags(std::uint8_t* at, std::uint32_t kind, std::uint64_t flags) {
    return KeepChunk(at, 0xFC000000u, kind | ((static_cast<std::uint32_t>(flags >> 32) & 0xFFFFFu) << 6), static_cast<std::uint32_t>(flags));
}

static std::uint8_t* BeginJob(std::uint8_t* job, const void* returnAddress) {
    std::uint8_t* at = job + 2 * sizeof(std::uint32_t);
    return returnAddress ? KeepBuffer(at, 6, returnAddress, 0) : at;
}

static void* EndJob(std::uint8_t* job, std::uint8_t* end, std::uint32_t instance) {
    KeepChunk(job, 0xFC000000u, (instance & 0xFFFFFu) << 6, static_cast<std::uint32_t>(end - job) - 2 * sizeof(std::uint32_t));
    return end;
}

void* APS5_VABI sceAjmBatchJobControlBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    auto* job = static_cast<std::uint8_t*>(position);
    auto* at = BeginJob(job, return_address);
    at = KeepBuffer(at, 2, sideband_input, sideband_input_size);
    at = KeepFlags(at, 3, flags & 0x000060000000E7FF);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    return EndJob(job, at, instance);
}

static void* RunJob(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address) {
    auto* job = static_cast<std::uint8_t*>(position);
    auto* at = BeginJob(job, return_address);
    for (std::size_t index = 0; index < input_buffers_num; ++index) at = KeepBuffer(at, 1, input_buffers[index].ptr, input_buffers[index].size);
    at = KeepFlags(at, 4, flags & 0x0000E00000001FFF);
    for (std::size_t index = 0; index < output_buffers_num; ++index) at = KeepBuffer(at, 0x11, output_buffers[index].ptr, output_buffers[index].size);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    return EndJob(job, at, instance);
}

void* APS5_VABI sceAjmBatchJobRunSplitBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address) {
    if ((input_buffers_num && !input_buffers) || (output_buffers_num && !output_buffers)) NotImplemented_nid_no_patch("AJM batch job with a null buffer list and a nonzero buffer count");
    return RunJob(position, instance, flags, input_buffers, input_buffers_num, output_buffers, output_buffers_num, sideband_output, sideband_output_size, return_address);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const AjmBuffer input{const_cast<void*>(data_input), data_input_size};
    const AjmBuffer output{data_output, data_output_size};
    return RunJob(position, instance, flags, &input, 1, &output, 1, sideband_output, sideband_output_size, return_address);
}

}

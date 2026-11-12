#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAjmi/include/InventoryExports.hpp"
#include "prx/libSceAjmi/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address) {
    if (data_input_size > std::numeric_limits<std::uint32_t>::max() - 7) throw std::invalid_argument("AJMI: an inline buffer of more than 4 GiB - 8 bytes does not fit the 32-bit job size");
    const std::size_t size = (data_input_size + 7) & ~std::size_t{7};
    auto* at = static_cast<std::uint8_t*>(position);
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & ~0x1Fu) | 7u;
    words[1] = static_cast<std::uint32_t>(size);
    std::memcpy(at, words, sizeof(words));
    at += sizeof(words);
    if (data_input_size) std::memcpy(at, data_input, data_input_size);
    if (batch_address) *batch_address = at;
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

extern "C" {

int APS5_VABI sceAjmiBatchCancel() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchWait() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI sceAjmiBatchJobInlineBuffer_nid_no_patch(void* position, const void* data, std::size_t size, const void** address) {
    return sceAjmBatchJobInlineBuffer_nid_disambig1(position, data, size, address);
}
APS5_EXPORT("4MsUK9kAnKg", sceAjmiBatchJobInlineBuffer_nid_no_patch);

int APS5_VABI sceAjmiModuleUnregister() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchStartBuffer() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiFinalize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiStrError() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchJobControlBufferRa() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInstanceSwitch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiDecAt9ParseConfigData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchErrorDump() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInstanceCodecType() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiMemoryUnregister() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInstanceDestroy() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiDecMp3ParseFrame() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiMemoryRegister() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiModuleRegister() {
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI sceAjmiBatchJobRunBufferRa_nid_no_patch(void* position, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, void* output, std::size_t outputSize, void* sideband, std::size_t sidebandSize, void* returnAddress) {
    return sceAjmBatchJobRunBufferRa_nid_disambig1(position, instance, flags, input, inputSize, output, outputSize, sideband, sidebandSize, returnAddress);
}
APS5_EXPORT("m0xisYC6U2w", sceAjmiBatchJobRunBufferRa_nid_no_patch);

int APS5_VABI sceAjmiInstanceCreate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInstanceExtend() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiBatchJobRunSplitBufferRa() {
    NotImplemented_nid_no_patch(__func__);
}

}

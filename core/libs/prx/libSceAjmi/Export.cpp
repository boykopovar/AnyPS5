#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAjmi/include/InventoryExports.hpp"
#include "prx/libSceAjmi/include/Api.hpp"
#include "prx/libSceAjm.native/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address) {
    if (data_input_size > std::numeric_limits<std::uint32_t>::max() - 7) throw std::invalid_argument("AJMI: an inline buffer of more than 4 GiB - 8 bytes does not fit the 32-bit job size");
    const std::size_t size = (data_input_size + 7) & ~std::size_t{7};
    const auto start = reinterpret_cast<std::uintptr_t>(position);
    if (start == 0 || start > std::numeric_limits<std::uintptr_t>::max() - sizeof(std::uint32_t) * 2 ||
        size > std::numeric_limits<std::uintptr_t>::max() - start - sizeof(std::uint32_t) * 2) {
        throw std::runtime_error("AJMI: inline batch destination address is invalid");
    }
    const auto inputStart = reinterpret_cast<std::uintptr_t>(data_input);
    if (data_input_size > 0 && (inputStart == 0 || data_input_size > std::numeric_limits<std::uintptr_t>::max() - inputStart)) {
        throw std::runtime_error("AJMI: inline input address is invalid");
    }
    const int savedErrno = errno;
    const auto read = [&](void* destination, const void* source, std::size_t bytes) {
        const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
        errno = savedErrno;
        return copied;
    };
    const auto write = [&](void* destination, const void* source, std::size_t bytes) {
        const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
        errno = savedErrno;
        return copied;
    };
    std::uint32_t words[2];
    if (!read(words, position, sizeof(words))) throw std::runtime_error("AJMI: inline batch header is unreadable");
    words[0] = (words[0] & ~0x1Fu) | 7u;
    words[1] = static_cast<std::uint32_t>(size);
    if (!write(position, words, sizeof(words))) throw std::runtime_error("AJMI: inline batch header is unwritable");
    const auto payload = start + sizeof(words);
    std::array<std::uint8_t, 4096> staging{};
    for (std::size_t offset = 0; offset < data_input_size;) {
        const std::size_t count = std::min(staging.size(), data_input_size - offset);
        if (!read(staging.data(), reinterpret_cast<const void*>(inputStart + offset), count)) {
            throw std::runtime_error("AJMI: inline input is unreadable");
        }
        if (!write(reinterpret_cast<void*>(payload + offset), staging.data(), count)) {
            throw std::runtime_error("AJMI: inline batch payload is unwritable");
        }
        offset += count;
    }
    const auto* address = reinterpret_cast<const void*>(payload);
    if (batch_address && !write(batch_address, &address, sizeof(address))) {
        throw std::runtime_error("AJMI: inline payload pointer output is unwritable");
    }
    return reinterpret_cast<void*>(payload + size);
}

static void ValidateGuestRange(const void* address, std::size_t bytes, const char* message) {
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (start == 0 || (bytes > 0 && bytes - 1 > std::numeric_limits<std::uintptr_t>::max() - start)) {
        throw std::runtime_error(message);
    }
}

static void ReadGuest(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (bytes == 0) return;
    ValidateGuestRange(source, bytes, message);
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

static void WriteGuest(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (bytes == 0) return;
    ValidateGuestRange(destination, bytes, message);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

static std::vector<AjmBuffer> ReadBufferList(const AjmBuffer* guestBuffers, std::size_t count, const char* message) {
    if (count == 0) return {};
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(AjmBuffer)) throw std::runtime_error(message);
    std::vector<AjmBuffer> buffers(count);
    ReadGuest(buffers.data(), guestBuffers, count * sizeof(AjmBuffer), message);
    return buffers;
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

static std::size_t EndJob(std::uint8_t* job, std::uint8_t* end, std::uint32_t instance) {
    const auto totalBytes = static_cast<std::size_t>(end - job);
    KeepChunk(job, 0xFC000000u, (instance & 0xFFFFFu) << 6, static_cast<std::uint32_t>(totalBytes) - 2 * sizeof(std::uint32_t));
    return totalBytes;
}

static constexpr std::size_t kJobHeaderBytes = 2 * sizeof(std::uint32_t);
static constexpr std::size_t kBufferEntryBytes = 2 * sizeof(std::uint32_t) + sizeof(const void*);

void* APS5_VABI sceAjmBatchJobControlBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const std::size_t totalBytes = kJobHeaderBytes + (return_address ? kBufferEntryBytes : 0) + 2 * kBufferEntryBytes + kJobHeaderBytes;
    ValidateGuestRange(position, totalBytes, "AJMI: batch job destination address is invalid");
    std::vector<std::uint8_t> staging(totalBytes);
    ReadGuest(staging.data(), position, totalBytes, "AJMI: batch job buffer is unreadable");
    auto* at = BeginJob(staging.data(), return_address);
    at = KeepBuffer(at, 2, sideband_input, sideband_input_size);
    at = KeepFlags(at, 3, flags & 0x000060000000E7FF);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    EndJob(staging.data(), at, instance);
    WriteGuest(position, staging.data(), totalBytes, "AJMI: batch job buffer is unwritable");
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(position) + totalBytes);
}

static void* RunJob(void* position, uint32_t instance, uint64_t flags, std::span<const AjmBuffer> input_buffers, std::span<const AjmBuffer> output_buffers, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const std::size_t fixedBytes = 2 * kJobHeaderBytes + (return_address ? kBufferEntryBytes : 0) + kBufferEntryBytes;
    if (input_buffers.size() > (std::numeric_limits<std::uint32_t>::max() - fixedBytes) / kBufferEntryBytes ||
        output_buffers.size() > (std::numeric_limits<std::uint32_t>::max() - fixedBytes - input_buffers.size() * kBufferEntryBytes) / kBufferEntryBytes) {
        throw std::runtime_error("AJMI: batch job size overflows");
    }
    const std::size_t totalBytes = fixedBytes + (input_buffers.size() + output_buffers.size()) * kBufferEntryBytes;
    ValidateGuestRange(position, totalBytes, "AJMI: batch job destination address is invalid");
    std::vector<std::uint8_t> staging(totalBytes);
    ReadGuest(staging.data(), position, totalBytes, "AJMI: batch job buffer is unreadable");
    auto* at = BeginJob(staging.data(), return_address);
    for (const AjmBuffer& input : input_buffers) at = KeepBuffer(at, 1, input.ptr, input.size);
    at = KeepFlags(at, 4, flags & 0x0000E00000001FFF);
    for (const AjmBuffer& output : output_buffers) at = KeepBuffer(at, 0x11, output.ptr, output.size);
    at = KeepBuffer(at, 0x12, sideband_output, sideband_output_size);
    EndJob(staging.data(), at, instance);
    WriteGuest(position, staging.data(), totalBytes, "AJMI: batch job buffer is unwritable");
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(position) + totalBytes);
}

void* APS5_VABI sceAjmBatchJobRunSplitBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const std::vector<AjmBuffer> inputs = ReadBufferList(input_buffers, input_buffers_num, "AJMI: batch input buffer array is unreadable");
    const std::vector<AjmBuffer> outputs = ReadBufferList(output_buffers, output_buffers_num, "AJMI: batch output buffer array is unreadable");
    return RunJob(position, instance, flags, inputs, outputs, sideband_output, sideband_output_size, return_address);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const std::array<AjmBuffer, 1> inputs{AjmBuffer{const_cast<void*>(data_input), data_input_size}};
    const std::array<AjmBuffer, 1> outputs{AjmBuffer{data_output, data_output_size}};
    return RunJob(position, instance, flags, inputs, outputs, sideband_output, sideband_output_size, return_address);
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

const char* APS5_VABI sceAjmiStrError(int error) {
    return sceAjmStrError(error);
}

int APS5_VABI sceAjmiBatchJobControlBufferRa() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiInstanceSwitch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAjmiDecAt9ParseConfigData(const void* configData, AjmDecAt9ConfigDataInfo* configInfo) {
    return sceAjmDecAt9ParseConfigData(configData, configInfo);
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

int APS5_VABI sceAjmiDecMp3ParseFrame(const std::uint8_t* stream, std::uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame) {
    return sceAjmDecMp3ParseFrame(stream, streamSize, parseOfl, frame);
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

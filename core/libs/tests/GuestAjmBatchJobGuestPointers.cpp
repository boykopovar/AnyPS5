#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int kInvalidParameter = static_cast<int>(0x80930005);

void Require(bool ok, const char* description) {
    if (ok) return;
    std::fprintf(stderr, "AJM batch job guest pointer: %s\n", description);
    std::abort();
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0, "get page size");
    return static_cast<std::size_t>(size);
#endif
}

char* GuardedPageEnd() {
    const std::size_t pageSize = PageSize();
#ifdef _WIN32
    auto* bytes = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(bytes != nullptr, "allocate guard pages");
    DWORD previous = 0;
    Require(VirtualProtect(bytes + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect second page");
#else
    void* memory = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(memory != MAP_FAILED, "allocate guard pages");
    auto* bytes = static_cast<char*>(memory);
    Require(mprotect(bytes + pageSize, pageSize, PROT_NONE) == 0, "protect second page");
#endif
    return bytes + pageSize;
}

char* ReadOnlyPage() {
    const std::size_t pageSize = PageSize();
#ifdef _WIN32
    auto* bytes = static_cast<char*>(VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(bytes != nullptr, "allocate read-only page");
#else
    void* memory = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(memory != MAP_FAILED, "allocate read-only page");
    auto* bytes = static_cast<char*>(memory);
#endif
    return bytes;
}

void ProtectReadOnly(char* page) {
    const std::size_t pageSize = PageSize();
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0, "protect read-only page");
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0, "protect read-only page");
#endif
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    char* end = GuardedPageEnd();
    char* readOnlyMemory = ReadOnlyPage();
    std::array<std::uint8_t, 512> batchBuffer{};
    batchBuffer.fill(0xA5);

    auto* readOnlyInfo = reinterpret_cast<AjmBatchInfo*>(readOnlyMemory);
    readOnlyInfo->p_buffer = batchBuffer.data();
    readOnlyInfo->offset = 0;
    readOnlyInfo->size = batchBuffer.size();
    auto* readOnlyOversizedInfo = reinterpret_cast<AjmBatchInfo*>(readOnlyMemory + sizeof(AjmBatchInfo));
    readOnlyOversizedInfo->p_buffer = batchBuffer.data();
    readOnlyOversizedInfo->offset = 0;
    readOnlyOversizedInfo->size = 8;
    ProtectReadOnly(readOnlyMemory);

    uint32_t context = 0;
    Require(sceAjmInitialize(0, &context) == 0, "initialize AJM context");
    uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 23, 0, &instance) == 0, "create PCM instance");

    std::uint64_t sidebandResult = 0;
    std::array<std::uint8_t, 16> inputData{};
    std::array<std::uint8_t, 16> outputData{};
    const AjmBuffer oneIn{inputData.data(), inputData.size()};
    const AjmBuffer oneOut{outputData.data(), outputData.size()};

    for (AjmBatchInfo* badInfo : {static_cast<AjmBatchInfo*>(nullptr), reinterpret_cast<AjmBatchInfo*>(0x10), reinterpret_cast<AjmBatchInfo*>(end - 8), readOnlyInfo}) {
        errno = 71;
        Require(sceAjmBatchJobInitialize(badInfo, instance, inputData.data(), 8, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by Initialize");
        Require(sceAjmBatchJobClearContext(badInfo, instance, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by ClearContext");
        Require(sceAjmBatchJobSetGaplessDecode(badInfo, instance, inputData.data(), 1, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by SetGaplessDecode");
        Require(sceAjmBatchJobControl(badInfo, instance, 1ull << 13, nullptr, 0, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && errno == 71, "bad info accepted by Control");
        Require(sceAjmBatchJobRunSplit(badInfo, instance, 0, &oneIn, 1, &oneOut, 1, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && errno == 71, "bad info accepted by RunSplit");
        Require(sceAjmBatchJobRun(badInfo, instance, 0, inputData.data(), inputData.size(), outputData.data(), outputData.size(), &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && errno == 71, "bad info accepted by Run");
        Require(sceAjmBatchJobSetResampleParameters(badInfo, instance, 1.0f, 0, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by SetResampleParameters");
        Require(sceAjmBatchJobSetResampleParametersEx(badInfo, instance, 1.0f, 0.0f, 0, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by SetResampleParametersEx");
        Require(sceAjmBatchJobGetResampleInfo(badInfo, instance, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by GetResampleInfo");
        Require(sceAjmBatchJobDecodeSplit(badInfo, instance, &oneIn, 1, &oneOut, 1, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by DecodeSplit");
        Require(sceAjmBatchJobGetGaplessDecode(badInfo, instance, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by GetGaplessDecode");
        Require(sceAjmBatchJobGetInfo(badInfo, instance, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by GetInfo");
        Require(sceAjmBatchJobGetCodecInfo(badInfo, instance, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && errno == 71, "bad info accepted by GetCodecInfo");
        Require(sceAjmBatchJobGetStatistics(badInfo, 1.0f, &sidebandResult) == kInvalidParameter && errno == 71, "bad info accepted by GetStatistics");
    }
    Require(batchBuffer[0] == 0xA5, "read-only info mutated batch buffer");
    errno = 71;
    Require(sceAjmBatchJobRunSplit(readOnlyOversizedInfo, instance, 0, &oneIn, 1, &oneOut, 1, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && errno == 71, "read-only oversized info accepted by RunSplit");

    AjmBatchInfo info{};
    for (void* badBuffer : {static_cast<void*>(reinterpret_cast<void*>(0x10)), static_cast<void*>(readOnlyMemory), static_cast<void*>(end - 8)}) {
        info.p_buffer = badBuffer;
        info.offset = 0;
        info.size = 256;
        errno = 71;
        Require(sceAjmBatchJobClearContext(&info, instance, &sidebandResult) == kInvalidParameter && info.offset == 0 && errno == 71, "unwritable batch buffer accepted");
    }

    batchBuffer.fill(0xA5);
    Require(sceAjmBatchInitialize(batchBuffer.data(), batchBuffer.size(), &info) == 0, "initialize valid batch info");

    std::memcpy(end - 4, inputData.data(), 4);
    for (const void* badParams : {static_cast<const void*>(reinterpret_cast<const void*>(0x10)), static_cast<const void*>(end - 4)}) {
        errno = 71;
        Require(sceAjmBatchJobInitialize(&info, instance, badParams, 8, &sidebandResult) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable codec_parameters accepted");
        Require(sceAjmBatchJobSetGaplessDecode(&info, instance, badParams, 1, &sidebandResult) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable gapless_decode accepted");
    }

    std::memcpy(end - 8, &oneIn, 8);
    for (const AjmBuffer* badList : {reinterpret_cast<const AjmBuffer*>(0x10), reinterpret_cast<const AjmBuffer*>(end - 8)}) {
        errno = 71;
        Require(sceAjmBatchJobRunSplit(&info, instance, 0, badList, 1, &oneOut, 1, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable input_buffers accepted by RunSplit");
        Require(sceAjmBatchJobRunSplit(&info, instance, 0, &oneIn, 1, badList, 1, &sidebandResult, sizeof(sidebandResult)) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable output_buffers accepted by RunSplit");
        Require(sceAjmBatchJobDecodeSplit(&info, instance, badList, 1, &oneOut, 1, &sidebandResult) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable input_buffers accepted by DecodeSplit");
        Require(sceAjmBatchJobDecodeSplit(&info, instance, &oneIn, 1, badList, 1, &sidebandResult) == kInvalidParameter && info.offset == 0 && errno == 71, "unreadable output_buffers accepted by DecodeSplit");
    }

    const std::uint32_t validParams[2] = {1u, 11u};
    std::memcpy(end - sizeof(validParams), validParams, sizeof(validParams));
    errno = 71;
    Require(sceAjmBatchJobInitialize(&info, instance, end - sizeof(validParams), sizeof(validParams), &sidebandResult) == 0 && info.offset == 40 && errno == 71, "valid codec_parameters at page end failed");

    const std::uint64_t gapless[1] = {0x0000000000000400ull};
    std::memcpy(end - sizeof(gapless), gapless, sizeof(gapless));
    errno = 71;
    Require(sceAjmBatchJobSetGaplessDecode(&info, instance, end - sizeof(gapless), 1, &sidebandResult) == 0 && info.offset == 80 && errno == 71, "valid gapless_decode at page end failed");

    std::memcpy(end - sizeof(AjmBuffer), &oneIn, sizeof(AjmBuffer));
    errno = 71;
    Require(sceAjmBatchJobRunSplit(&info, instance, 0, reinterpret_cast<const AjmBuffer*>(end - sizeof(AjmBuffer)), 1, reinterpret_cast<const AjmBuffer*>(end - sizeof(AjmBuffer)), 1, &sidebandResult, sizeof(sidebandResult)) == 0 && info.offset == 144 && errno == 71, "valid RunSplit buffer lists at page end failed");

    std::array<std::uint8_t, 512> startStorage{};
    AjmBatchInfo startInfo{};
    Require(sceAjmBatchInitialize(startStorage.data(), startStorage.size(), &startInfo) == 0, "initialize start batch");
    std::uint64_t initSideband = 0xA5A5A5A5A5A5A5A5ull;
    const std::array<std::uint32_t, 3> pcmParams{0, 1, 48000};
    Require(sceAjmBatchJobInitialize(&startInfo, instance, pcmParams.data(), sizeof(pcmParams), &initSideband) == 0, "queue Initialize job");
    const std::size_t oneJobBytes = startInfo.offset;
    uint32_t batchId = 0;
    AjmBatchError batchError{};

    for (const AjmBatchInfo* badStartInfo : {reinterpret_cast<const AjmBatchInfo*>(std::uintptr_t{0x10}), reinterpret_cast<const AjmBatchInfo*>(end - 8)}) {
        errno = 71;
        Require(sceAjmBatchStart(context, badStartInfo, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == kInvalidParameter && errno == 71, "bad batch info accepted by Start");
    }
    for (uint32_t* badBatchId : {reinterpret_cast<uint32_t*>(std::uintptr_t{0x10}), reinterpret_cast<uint32_t*>(readOnlyMemory), reinterpret_cast<uint32_t*>(end - 2)}) {
        errno = 71;
        Require(sceAjmBatchStart(context, &startInfo, SCE_AJM_PRIORITY_MIN, &batchError, badBatchId) == kInvalidParameter && errno == 71, "unwritable batch id accepted by Start");
        Require(initSideband == 0xA5A5A5A5A5A5A5A5ull, "Start executed jobs despite batch registration failure");
    }
    for (AjmBatchError* badError : {reinterpret_cast<AjmBatchError*>(std::uintptr_t{0x10}), reinterpret_cast<AjmBatchError*>(readOnlyMemory), reinterpret_cast<AjmBatchError*>(end - 8)}) {
        errno = 71;
        Require(sceAjmBatchStart(context, &startInfo, SCE_AJM_PRIORITY_MIN, badError, &batchId) == kInvalidParameter && errno == 71, "unwritable error accepted by Start");
        Require(initSideband == 0xA5A5A5A5A5A5A5A5ull, "Start executed jobs despite error output failure");
    }

    std::array<std::uint8_t, 512> partialStorage{};
    AjmBatchInfo partialSrc{};
    Require(sceAjmBatchInitialize(partialStorage.data(), partialStorage.size(), &partialSrc) == 0, "initialize two-job batch");
    Require(sceAjmBatchJobInitialize(&partialSrc, instance, pcmParams.data(), sizeof(pcmParams), &initSideband) == 0, "queue first job for partial test");
    std::uint64_t clearSideband = 0xA5A5A5A5A5A5A5A5ull;
    Require(sceAjmBatchJobClearContext(&partialSrc, instance, &clearSideband) == 0, "queue second job for partial test");
    const std::size_t mappedBytes = oneJobBytes + 8;
    std::memcpy(end - mappedBytes, partialStorage.data(), mappedBytes);
    AjmBatchInfo partialInfo{end - mappedBytes, partialSrc.offset, partialStorage.size()};
    errno = 71;
    Require(sceAjmBatchStart(context, &partialInfo, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == kInvalidParameter && errno == 71, "partially mapped batch accepted by Start");
    Require(initSideband == 0xA5A5A5A5A5A5A5A5ull, "Start executed first job when second job faulted");

    errno = 71;
    Require(sceAjmBatchStart(context, &startInfo, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == 0 && batchId != 0 && errno == 71, "valid Start failed");
    Require(initSideband == 0, "valid Initialize job did not update sideband");
    for (AjmBatchError* badError : {reinterpret_cast<AjmBatchError*>(std::uintptr_t{0x10}), reinterpret_cast<AjmBatchError*>(readOnlyMemory), reinterpret_cast<AjmBatchError*>(end - 8)}) {
        errno = 71;
        Require(sceAjmBatchWait(context, batchId, 0, badError) == kInvalidParameter && errno == 71, "unwritable error accepted by Wait");
    }
    errno = 71;
    Require(sceAjmBatchWait(context, batchId, 0, &batchError) == 0 && errno == 71, "valid Wait failed after unwritable error probe");
    constexpr int kInvalidBatch = static_cast<int>(0x80930004);
    errno = 71;
    Require(sceAjmBatchWait(context, batchId, 0, nullptr) == kInvalidBatch && errno == 71, "completed batch stayed registered");

    const auto appendBufferChunk = [](std::array<std::uint32_t, 32>& words, std::size_t index, std::uint32_t kind, const void* pointer, std::size_t size) {
        words[index] = kind;
        words[index + 1] = static_cast<std::uint32_t>(size);
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        words[index + 2] = static_cast<std::uint32_t>(address);
        words[index + 3] = static_cast<std::uint32_t>(address >> 32);
    };
    std::array<std::uint8_t, 24> streamSideband{};
    streamSideband.fill(0xA5);
    std::array<std::uint32_t, 32> consoleJobs{};
    consoleJobs[0] = instance << 6;
    consoleJobs[1] = 56;
    appendBufferChunk(consoleJobs, 2, 1, inputData.data(), sizeof(inputData));
    consoleJobs[6] = 4 | (0x8000u << 6);
    consoleJobs[7] = 0;
    appendBufferChunk(consoleJobs, 8, 17, outputData.data(), sizeof(outputData));
    appendBufferChunk(consoleJobs, 12, 18, streamSideband.data(), streamSideband.size());
    for (std::size_t i = 0; i < 16; ++i) consoleJobs[16 + i] = consoleJobs[i];
    const auto* consoleBytes = reinterpret_cast<const std::uint8_t*>(consoleJobs.data());

    for (const std::uint8_t* badBatch : {reinterpret_cast<const std::uint8_t*>(std::uintptr_t{0x10}), reinterpret_cast<const std::uint8_t*>(end - 4)}) {
        errno = 71;
        Require(sceAjmBatchStartBuffer(context, badBatch, 8, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == kInvalidParameter && errno == 71, "unreadable buffer accepted by StartBuffer");
    }
    const std::size_t mappedConsole = 64 + 12;
    std::memcpy(end - mappedConsole, consoleBytes, mappedConsole);
    errno = 71;
    Require(sceAjmBatchStartBuffer(context, reinterpret_cast<const std::uint8_t*>(end - mappedConsole), 128, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == kInvalidParameter && errno == 71, "second console job faulting in ReadConsoleJob accepted by StartBuffer");
    Require(streamSideband[0] == 0xA5, "StartBuffer executed first console job when second job faulted");

    const std::uint32_t malformedWords[2]{instance << 6, 4};
    for (AjmBatchError* badError : {reinterpret_cast<AjmBatchError*>(std::uintptr_t{0x10}), reinterpret_cast<AjmBatchError*>(readOnlyMemory), reinterpret_cast<AjmBatchError*>(end - 8)}) {
        errno = 71;
        Require(sceAjmBatchStartBuffer(context, reinterpret_cast<const std::uint8_t*>(malformedWords), sizeof(malformedWords), SCE_AJM_PRIORITY_MIN, badError, &batchId) == kInvalidParameter && errno == 71, "malformed StartBuffer wrote unwritable error");
    }
    for (uint32_t* badBatchId : {reinterpret_cast<uint32_t*>(std::uintptr_t{0x10}), reinterpret_cast<uint32_t*>(readOnlyMemory), reinterpret_cast<uint32_t*>(end - 2)}) {
        errno = 71;
        Require(sceAjmBatchStartBuffer(context, consoleBytes, 64, SCE_AJM_PRIORITY_MIN, &batchError, badBatchId) == kInvalidParameter && errno == 71, "StartBuffer accepted unwritable batch id");
        Require(streamSideband[0] == 0xA5, "StartBuffer executed job despite batch registration failure");
    }
    batchId = 0;
    errno = 71;
    Require(sceAjmBatchStartBuffer(context, consoleBytes, 64, SCE_AJM_PRIORITY_MIN, &batchError, &batchId) == 0 && batchId != 0 && errno == 71, "valid StartBuffer failed");
    Require(sceAjmBatchWait(context, batchId, 0, &batchError) == 0, "wait for valid StartBuffer batch failed");
    Require(streamSideband[0] == 0, "valid StartBuffer job did not write sideband");

    std::array<std::uint8_t, 256> throwStorage{};
    AjmBatchInfo throwInfo{};
    Require(sceAjmBatchInitialize(throwStorage.data(), throwStorage.size(), &throwInfo) == 0, "initialize throwing batch");
    Require(sceAjmBatchJobGetStatistics(&throwInfo, 1.0f, reinterpret_cast<void*>(std::uintptr_t{0x10})) == 0, "queue throwing statistics job");
    std::uint32_t thrownStartId = 0;
    bool startThrew = false;
    try {
        sceAjmBatchStart(context, &throwInfo, SCE_AJM_PRIORITY_MIN, &batchError, &thrownStartId);
    } catch (const std::runtime_error&) {
        startThrew = true;
    }
    Require(startThrew && thrownStartId != 0, "Start execution fault did not throw");
    Require(sceAjmBatchWait(context, thrownStartId, 0, nullptr) == kInvalidBatch, "failed Start left batch registered");
    Require(sceAjmBatchCancel(context, thrownStartId) == kInvalidBatch, "failed Start left batch cancellable");

    std::array<std::uint32_t, 32> throwConsole{};
    throwConsole[0] = 0x80000u << 6;
    throwConsole[1] = 24;
    throwConsole[2] = 4;
    throwConsole[3] = 0;
    appendBufferChunk(throwConsole, 4, 18, reinterpret_cast<void*>(std::uintptr_t{0x10}), 24);
    std::uint32_t thrownBufferId = 0;
    bool bufferThrew = false;
    try {
        sceAjmBatchStartBuffer(context, reinterpret_cast<const std::uint8_t*>(throwConsole.data()), 32, SCE_AJM_PRIORITY_MIN, &batchError, &thrownBufferId);
    } catch (const std::runtime_error&) {
        bufferThrew = true;
    }
    Require(bufferThrew && thrownBufferId != 0, "StartBuffer execution fault did not throw");
    Require(sceAjmBatchWait(context, thrownBufferId, 0, nullptr) == kInvalidBatch, "failed StartBuffer left batch registered");
    Require(sceAjmBatchCancel(context, thrownBufferId) == kInvalidBatch, "failed StartBuffer left batch cancellable");

    Require(sceAjmInstanceDestroy(context, instance) == 0, "destroy PCM instance");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("AJM batch job guest pointer tests passed");
    return 0;
}

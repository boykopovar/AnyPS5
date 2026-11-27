#include "prx/libSceAjm.native/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    Require(sceAjmInstanceDestroy(context, instance) == 0, "destroy PCM instance");
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("AJM batch job guest pointer tests passed");
    return 0;
}

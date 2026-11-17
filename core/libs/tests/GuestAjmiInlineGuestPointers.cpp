#include "prx/libSceAjmi/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

char* GuardedPageEnd() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* bytes = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(bytes != nullptr, "allocate guard pages");
    DWORD previous = 0;
    Require(VirtualProtect(bytes + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect second page");
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0, "get page size");
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* memory = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(memory != MAP_FAILED, "allocate guard pages");
    auto* bytes = static_cast<char*>(memory);
    Require(mprotect(bytes + pageSize, pageSize, PROT_NONE) == 0, "protect second page");
#endif
    return bytes + pageSize;
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    std::array<std::uint8_t, 32> job{};
    job.fill(0xA5);
    const std::array<std::uint8_t, 5> data{4, 6, 8, 10, 12};
    const void* payload = nullptr;
    errno = 71;
    ExpectErr<std::runtime_error>([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(reinterpret_cast<void*>(0x10), nullptr, 0, nullptr); }, "unreadable destination header accepted");
    Require(errno == 71, "invalid destination changed errno");
    ExpectErr<std::runtime_error>([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), reinterpret_cast<const void*>(0x10), 5, nullptr); }, "unreadable data input accepted");
    Require(errno == 71, "invalid source changed errno");
    ExpectErr<std::runtime_error>([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), data.data(), data.size(), reinterpret_cast<const void**>(0x10)); }, "unwritable payload address accepted");
    Require(errno == 71, "invalid address output changed errno");

    job.fill(0xA5);
    void* next = sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), data.data(), data.size(), &payload);
    Require(next == job.data() + 16, "next chunk pointer changed");
    Require(payload == job.data() + 8, "payload pointer changed");
    const std::array<std::uint8_t, 16> expected{0xA7, 0xA5, 0xA5, 0xA5, 8, 0, 0, 0, 4, 6, 8, 10, 12, 0xA5, 0xA5, 0xA5};
    Require(std::memcmp(job.data(), expected.data(), expected.size()) == 0, "measured buffer bytes changed");
    Require(job[16] == 0xA5, "output exceeded the expected chunk");
    Require(errno == 71, "valid inline encoding changed errno");

    char* end = GuardedPageEnd();
    std::memset(end - 16, 0xA5, 16);
    Require(sceAjmiBatchJobInlineBuffer_nid_no_patch(end - 16, data.data(), data.size(), nullptr) == end, "batch at writable page end was refused");
    ExpectErr<std::runtime_error>([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(end - 12, data.data(), data.size(), nullptr); }, "batch writing past page boundary was accepted");
    std::memcpy(end - 4, data.data(), 4);
    ExpectErr<std::runtime_error>([&] { sceAjmiBatchJobInlineBuffer_nid_no_patch(job.data(), end - 4, 5, nullptr); }, "input crossing page boundary was accepted");

    std::array<std::uint8_t, 128> batchJob{};
    batchJob.fill(0xA5);
    std::array<std::uint8_t, 16> inputSample{};
    std::array<std::uint8_t, 16> outputSample{};
    std::array<std::uint8_t, 8> sidebandSample{};
    for (void* badPosition : {static_cast<void*>(nullptr), reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10)), static_cast<void*>(end - 8)}) {
        if (badPosition == end - 8) std::memset(end - 8, 0x5A, 8);
        errno = 71;
        ExpectErr<std::runtime_error>([&] {
            sceAjmBatchJobControlBufferRa_nid_disambig1(badPosition, 1, 0, inputSample.data(), inputSample.size(), sidebandSample.data(), sidebandSample.size(), nullptr);
        }, "invalid ControlBufferRa position accepted");
        Require(errno == 71, "ControlBufferRa bad position changed errno");
        if (badPosition == end - 8) Require(static_cast<std::uint8_t>(end[-8]) == 0x5A, "ControlBufferRa partially wrote page end");
        errno = 71;
        ExpectErr<std::runtime_error>([&] {
            sceAjmBatchJobRunBufferRa_nid_disambig1(badPosition, 1, 0, inputSample.data(), inputSample.size(), outputSample.data(), outputSample.size(), nullptr, 0, nullptr);
        }, "invalid RunBufferRa position accepted");
        Require(errno == 71, "RunBufferRa bad position changed errno");
    }

    const AjmBuffer oneIn{inputSample.data(), inputSample.size()};
    const AjmBuffer oneOut{outputSample.data(), outputSample.size()};
    for (const AjmBuffer* badList : {static_cast<const AjmBuffer*>(nullptr), reinterpret_cast<const AjmBuffer*>(static_cast<std::uintptr_t>(0x10)), reinterpret_cast<const AjmBuffer*>(end - 8)}) {
        errno = 71;
        ExpectErr<std::runtime_error>([&] {
            sceAjmBatchJobRunSplitBufferRa_nid_disambig1(batchJob.data(), 1, 0, badList, 1, &oneOut, 1, nullptr, 0, nullptr);
        }, "invalid SplitBufferRa input_buffers accepted");
        Require(errno == 71 && batchJob[0] == 0xA5, "SplitBufferRa bad input_buffers changed errno or job");
        errno = 71;
        ExpectErr<std::runtime_error>([&] {
            sceAjmBatchJobRunSplitBufferRa_nid_disambig1(batchJob.data(), 1, 0, &oneIn, 1, badList, 1, nullptr, 0, nullptr);
        }, "invalid SplitBufferRa output_buffers accepted");
        Require(errno == 71 && batchJob[0] == 0xA5, "SplitBufferRa bad output_buffers changed errno or job");
    }

    batchJob.fill(0);
    errno = 71;
    void* ctrlEnd = sceAjmBatchJobControlBufferRa_nid_disambig1(batchJob.data(), 2, 1, inputSample.data(), inputSample.size(), sidebandSample.data(), sidebandSample.size(), nullptr);
    Require(ctrlEnd == batchJob.data() + 48 && errno == 71, "valid ControlBufferRa failed or changed errno");
    batchJob.fill(0);
    errno = 71;
    void* runEnd = sceAjmiBatchJobRunBufferRa_nid_no_patch(batchJob.data(), 3, 1, inputSample.data(), inputSample.size(), outputSample.data(), outputSample.size(), sidebandSample.data(), sidebandSample.size(), nullptr);
    Require(runEnd == batchJob.data() + 64 && errno == 71, "valid RunBufferRa failed or changed errno");
    batchJob.fill(0);
    errno = 71;
    void* splitEnd = sceAjmBatchJobRunSplitBufferRa_nid_disambig1(batchJob.data(), 4, 1, &oneIn, 1, &oneOut, 1, nullptr, 0, nullptr);
    Require(splitEnd == batchJob.data() + 64 && errno == 71, "valid SplitBufferRa failed or changed errno");

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("AJMI inline guest pointer tests passed");
}

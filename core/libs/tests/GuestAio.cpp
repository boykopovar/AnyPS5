#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#else
#include <sys/mman.h>
#endif

static void DeleteAio(std::int32_t id) {
    std::int32_t removed = -1;
    Require(sceKernelAioDeleteRequest(id, &removed) == 0 && removed == 0);
}

static void* MapReadOnlyPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READONLY);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

static void UnmapReadOnlyPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

#ifdef _WIN32
static int g_parameterReports = 0;
static void CountParameterReport(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++g_parameterReports;
}
static void CheckBadPositionedDescriptors() {
    const auto file = std::filesystem::path("anyps5-aio-bad-descriptor-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    { std::ofstream stream(file, std::ios::binary); stream << "data"; Require(static_cast<bool>(stream)); }
    const int closed = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(closed >= 0 && sceKernelClose(closed) == 0);
    const auto previous = _set_thread_local_invalid_parameter_handler(CountParameterReport);
    for (int descriptor : {-1, closed}) {
        for (std::size_t count : {std::size_t{0}, std::size_t{1}}) {
            char buffer = 'x';
            Require(sceKernelPread(descriptor, &buffer, count, 0) == SCE_KERNEL_ERROR_EBADF);
            Require(sceKernelPwrite(descriptor, &buffer, count, 0) == SCE_KERNEL_ERROR_EBADF);
            for (bool write : {false, true}) {
                KernelAioResult result{-1, 0};
                KernelAioRwRequest request{0, count, &buffer, &result, descriptor};
                std::int32_t id = 0;
                Require((write ? sceKernelAioSubmitWriteCommands(&request, 1, 1, &id)
                             : sceKernelAioSubmitReadCommands(&request, 1, 1, &id)) == 0);
                Require(result.state == 3 && result.return_value == SCE_KERNEL_ERROR_EBADF);
                std::int32_t removed = -1;
                Require(sceKernelAioDeleteRequest(id, &removed) == 0);
            }
            Require(buffer == 'x');
            Require(g_parameterReports == 0);
        }
    }
    Require(::_get_osfhandle(closed) == -1);
    Require(g_parameterReports == 1);
    Require(_set_thread_local_invalid_parameter_handler(previous) == CountParameterReport);
    Require(std::filesystem::remove(file));
}
#endif
int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc > 1 && std::strcmp(argv[1], "bad-descriptors") == 0) {
        CheckBadPositionedDescriptors();
        return 0;
    }
#endif
    const auto root = std::filesystem::path("anyps5-aio-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "data.bin";
    const std::string seed = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    { std::ofstream stream(file, std::ios::binary); stream << seed; }
    static_assert(sizeof(KernelAioSchedulingParam) == 20);
    std::array<KernelAioSchedulingParam, 3> defaults{};
    defaults.fill({7, 7, 7, 7, 7});
    sceKernelAioInitializeParam(defaults.data());
    Require(defaults[0].schedulingWindowSize == 32 && defaults[0].delayedCountLimit == 32 && defaults[0].enableSplit == 1);
    Require(defaults[0].splitSize == 0x100000 && defaults[0].splitChunkSize == 0x100000);
    Require(defaults[1].schedulingWindowSize == 32 && defaults[1].delayedCountLimit == 32 && defaults[1].enableSplit == 1);
    Require(defaults[1].splitSize == 0x100000 && defaults[1].splitChunkSize == 0x100000);
    Require(defaults[2].schedulingWindowSize == 32 && defaults[2].delayedCountLimit == 32 && defaults[2].enableSplit == 0);
    Require(defaults[2].splitSize == 0 && defaults[2].splitChunkSize == 0);
    Require(sceKernelAioInitializeImpl(defaults.data(), 0x3c) == 0);
    KernelAioSchedulingParam consoleParam{7, 7, 7, 7, 7};
    Require(sceKernelAioSetParam(&consoleParam, 1, 1, 0, 0, 0) == 0);
    Require(consoleParam.schedulingWindowSize == 1 && consoleParam.delayedCountLimit == 1 && consoleParam.enableSplit == 0);
    Require(consoleParam.splitSize == 0 && consoleParam.splitChunkSize == 0);
    Require(sceKernelAioSetParam(&consoleParam, 4, 4, 1, 4096, 4096) == 0);
    Require(consoleParam.schedulingWindowSize == 4 && consoleParam.delayedCountLimit == 4 && consoleParam.enableSplit == 1);
    Require(consoleParam.splitSize == 4096 && consoleParam.splitChunkSize == 4096);
    std::array<KernelAioSchedulingParam, 3> schedulingParams{};
    schedulingParams.fill({7, 7, 7, 7, 7});
    Require(sceKernelAioSetParam(&schedulingParams[1], 64, 16, 1, 0x200000, 0x40000) == 0);
    Require(schedulingParams[1].schedulingWindowSize == 64 && schedulingParams[1].delayedCountLimit == 16);
    Require(schedulingParams[1].enableSplit == 1 && schedulingParams[1].splitSize == 0x200000 && schedulingParams[1].splitChunkSize == 0x40000);
    Require(schedulingParams[0].schedulingWindowSize == 7 && schedulingParams[0].splitChunkSize == 7);
    Require(schedulingParams[2].schedulingWindowSize == 7 && schedulingParams[2].splitChunkSize == 7);
    Require(sceKernelAioSetParam(&schedulingParams[2], 128, 128, 0, 0x1000000, 0x1000000) == 0);
    Require(schedulingParams[2].schedulingWindowSize == 128 && schedulingParams[2].delayedCountLimit == 128);
    Require(schedulingParams[2].enableSplit == 0 && schedulingParams[2].splitSize == 0x1000000 && schedulingParams[2].splitChunkSize == 0x1000000);
    const auto setParamRefuses = [&](std::int32_t window, std::int32_t delayed, std::uint32_t split, std::uint32_t size, std::uint32_t chunk) {
        KernelAioSchedulingParam untouchedParam{7, 7, 7, 7, 7};
        return sceKernelAioSetParam(&untouchedParam, window, delayed, split, size, chunk) == SCE_KERNEL_ERROR_EINVAL &&
            untouchedParam.schedulingWindowSize == 7 && untouchedParam.delayedCountLimit == 7 &&
            untouchedParam.enableSplit == 7 && untouchedParam.splitSize == 7 && untouchedParam.splitChunkSize == 7;
    };
    Require(setParamRefuses(129, 32, 1, 0x100000, 0x100000));
    Require(setParamRefuses(0x7fffffff, 32, 0, 0, 0));
    Require(setParamRefuses(-1, 32, 1, 0x100000, 0x100000));
    Require(setParamRefuses(32, 129, 1, 0x100000, 0x100000));
    Require(setParamRefuses(32, -1, 1, 0x100000, 0x100000));
    Require(setParamRefuses(32, 32, 2, 0x100000, 0x100000));
    Require(setParamRefuses(32, 32, 0xffffffffu, 0x100000, 0x100000));
    Require(setParamRefuses(32, 32, 1, 0x1000001, 0x100000));
    Require(setParamRefuses(32, 32, 1, 0x100000, 0x1000001));
    Require(setParamRefuses(32, 32, 1, 0, 0x100000));
    Require(setParamRefuses(32, 32, 1, 0x100000, 0));
    KernelAioSchedulingParam accepted{7, 7, 7, 7, 7};
    Require(sceKernelAioSetParam(&accepted, 0, 0, 0, 0, 0) == 0 && accepted.schedulingWindowSize == 0 && accepted.splitSize == 0);
    Require(sceKernelAioSetParam(&accepted, 1, 1, 0, 0xffffffffu, 0x1000001) == 0 && accepted.splitSize == 0xffffffffu && accepted.splitChunkSize == 0x1000001);
    Require(sceKernelAioSetParam(&accepted, 1, 1, 1, 1, 0x20000) == 0 && accepted.splitSize == 1 && accepted.splitChunkSize == 0x20000);
    Require(sceKernelAioSetParam(&accepted, 1, 1, 1, 0x10001, 0x1000000) == 0 && accepted.splitSize == 0x10001);
    bool nullThrew = false;
    try { sceKernelAioSetParam(nullptr, 32, 32, 1, 0x100000, 0x100000); } catch (const std::invalid_argument&) { nullThrew = true; }
    Require(nullThrew);
    auto* const badParam = reinterpret_cast<KernelAioSchedulingParam*>(1);
    Require(sceKernelAioSetParam(badParam, 32, 32, 1, 0x100000, 0x100000) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioSetParam(badParam, -1, 32, 1, 0x100000, 0x100000) == SCE_KERNEL_ERROR_EINVAL);
    bool initializeThrew = false;
    try { sceKernelAioInitializeParam(badParam); } catch (const std::invalid_argument&) { initializeThrew = true; }
    Require(initializeThrew);
    Require(sceKernelAioInitializeImpl(schedulingParams.data(), static_cast<std::int32_t>(sizeof(schedulingParams))) == 0);
    std::array<KernelAioSchedulingParam, 4> initParam{};
    initParam.fill({7, 7, 7, 7, 7});
    sceKernelAioInitializeParam(initParam.data());
    for (int index = 0; index < 2; ++index) {
        Require(initParam[index].schedulingWindowSize == 32 && initParam[index].delayedCountLimit == 32 && initParam[index].enableSplit == 1 &&
            initParam[index].splitSize == 0x100000 && initParam[index].splitChunkSize == 0x100000);
    }
    Require(initParam[2].schedulingWindowSize == 32 && initParam[2].delayedCountLimit == 32 && initParam[2].enableSplit == 0 &&
        initParam[2].splitSize == 0 && initParam[2].splitChunkSize == 0);
    Require(initParam[3].schedulingWindowSize == 7 && initParam[3].splitChunkSize == 7);
    for (const auto submit : {sceKernelAioSubmitReadCommands, sceKernelAioSubmitWriteCommands,
                             sceKernelAioSubmitReadCommandsMultiple, sceKernelAioSubmitWriteCommandsMultiple}) {
        auto* const unreadable = reinterpret_cast<KernelAioRwRequest*>(1);
        std::int32_t untouchedId = -1;
        Require(submit(unreadable, 0, 1, &untouchedId) == SCE_KERNEL_ERROR_EINVAL && untouchedId == -1);
        Require(submit(unreadable, 1, 1, &untouchedId) == SCE_KERNEL_ERROR_EFAULT && untouchedId == -1);
    }
    const int fd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    std::array<char, 16> first{};
    std::array<char, 16> second{};
    KernelAioResult firstResult{-1, 0};
    KernelAioResult secondResult{-1, 0};
    KernelAioRwRequest requests[2] = {
        {0, first.size(), first.data(), &firstResult, fd},
        {32, second.size(), second.data(), &secondResult, fd},
    };
    std::int32_t id = 0;
    Require(sceKernelAioSubmitReadCommands(requests, 2, 1, &id) == 0);
    Require(id > 0);
    std::int32_t state = 0;
    Require(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Require(state == 3);
    Require(firstResult.state == 3 && firstResult.return_value == 16);
    Require(secondResult.state == 3 && secondResult.return_value == 16);
    Require(std::memcmp(first.data(), seed.data(), 16) == 0);
    Require(std::memcmp(second.data(), seed.data() + 32, 16) == 0);
    KernelAioResult invalidReadResult{-1, 0};
    KernelAioRwRequest invalidReadRequest{-1, first.size(), first.data(), &invalidReadResult, fd};
    std::int32_t invalidReadId = 0;
    Require(sceKernelAioSubmitReadCommands(&invalidReadRequest, 1, 1, &invalidReadId) == 0);
    std::int32_t invalidReadState = 0;
    Require(sceKernelAioWaitRequest(invalidReadId, &invalidReadState, nullptr) == 0);
    Require(invalidReadState == 3);
    Require(invalidReadResult.state == 3 && invalidReadResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    std::array<char, 4> aggregateReadBuffer{};
    KernelAioResult aggregateReadSuccessResult{-1, 0};
    KernelAioResult aggregateReadErrorResult{-1, 0};
    KernelAioRwRequest aggregateReads[2] = {
        {4, aggregateReadBuffer.size(), aggregateReadBuffer.data(), &aggregateReadSuccessResult, fd},
        {-1, aggregateReadBuffer.size(), aggregateReadBuffer.data(), &aggregateReadErrorResult, fd},
    };
    std::int32_t aggregateReadId = 0;
    Require(sceKernelAioSubmitReadCommands(aggregateReads, 2, 1, &aggregateReadId) == 0);
    std::int32_t aggregateReadState = 0;
    Require(sceKernelAioWaitRequest(aggregateReadId, &aggregateReadState, nullptr) == 0);
    Require(aggregateReadState == 3);
    Require(aggregateReadSuccessResult.state == 3 && aggregateReadSuccessResult.return_value == 4);
    Require(aggregateReadErrorResult.state == 3 && aggregateReadErrorResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    Require(std::memcmp(aggregateReadBuffer.data(), seed.data() + 4, aggregateReadBuffer.size()) == 0);
    std::int32_t pollFirstId = 0;
    Require(sceKernelAioSubmitReadCommands(requests, 1, 1, &pollFirstId) == 0);
    std::int32_t pollFirst = 0;
    std::int32_t cancelFirst = 0;
    Require(sceKernelAioCancelRequest(pollFirstId, &cancelFirst) == 0 && cancelFirst == 3);
    Require(sceKernelAioPollRequest(pollFirstId, &pollFirst) == 0 && pollFirst == 3);
    Require(sceKernelAioPollRequest(pollFirstId, &pollFirst) == 0 && pollFirst == 0x10003);
    Require(sceKernelAioCancelRequest(pollFirstId, &cancelFirst) == 0 && cancelFirst == 3);
    Require(sceKernelAioDeleteRequest(pollFirstId, &cancelFirst) == 0 && cancelFirst == 0);
    std::int32_t polled = 0;
    Require(sceKernelAioPollRequest(id, &polled) == 0);
    Require(polled == 0x10003);
    Require(sceKernelAioWaitRequest(id, &state, nullptr) == 0 && state == 0x10003);
    std::int32_t deleted = -1;
    Require(sceKernelAioDeleteRequest(id, &deleted) == 0);
    Require(deleted == 0);
    Require(sceKernelClose(fd) == 0);
    const int writeFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(writeFd >= 0);
    std::array<char, 5> written = {'H', 'E', 'L', 'L', 'O'};
    KernelAioResult writeResult{-1, 0};
    KernelAioRwRequest writeRequest{0, written.size(), written.data(), &writeResult, writeFd};
    std::int32_t writeId = 0;
    Require(sceKernelAioSubmitWriteCommands(&writeRequest, 1, 1, &writeId) == 0);
    std::int32_t writeState = 0;
    Require(sceKernelAioWaitRequest(writeId, &writeState, nullptr) == 0);
    Require(writeState == 3 && writeResult.state == 3 && writeResult.return_value == 5);
    KernelAioResult invalidWriteResult{-1, 0};
    KernelAioRwRequest invalidWriteRequest{-1, written.size(), written.data(), &invalidWriteResult, writeFd};
    std::int32_t invalidWriteId = 0;
    Require(sceKernelAioSubmitWriteCommands(&invalidWriteRequest, 1, 1, &invalidWriteId) == 0);
    std::int32_t invalidWriteState = 0;
    Require(sceKernelAioWaitRequest(invalidWriteId, &invalidWriteState, nullptr) == 0);
    Require(invalidWriteState == 3);
    Require(invalidWriteResult.state == 3 && invalidWriteResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    std::array<char, 4> aggregateWriteBuffer = {'G', 'O', 'O', 'D'};
    KernelAioResult aggregateWriteSuccessResult{-1, 0};
    KernelAioResult aggregateWriteErrorResult{-1, 0};
    KernelAioRwRequest aggregateWrites[2] = {
        {8, aggregateWriteBuffer.size(), aggregateWriteBuffer.data(), &aggregateWriteSuccessResult, writeFd},
        {-1, aggregateWriteBuffer.size(), aggregateWriteBuffer.data(), &aggregateWriteErrorResult, writeFd},
    };
    std::int32_t aggregateWriteId = 0;
    Require(sceKernelAioSubmitWriteCommands(aggregateWrites, 2, 1, &aggregateWriteId) == 0);
    std::int32_t aggregateWriteState = 0;
    Require(sceKernelAioWaitRequest(aggregateWriteId, &aggregateWriteState, nullptr) == 0);
    Require(aggregateWriteState == 3);
    Require(aggregateWriteSuccessResult.state == 3 && aggregateWriteSuccessResult.return_value == 4);
    Require(aggregateWriteErrorResult.state == 3 && aggregateWriteErrorResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelClose(writeFd) == 0);
    { std::ifstream stream(file, std::ios::binary); std::string contents; std::getline(stream, contents);
      Require(contents.rfind("HELLO", 0) == 0); }
    Require(std::filesystem::file_size(file) == seed.size());
    { std::ifstream stream(file, std::ios::binary);
      std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
      Require(contents.size() == seed.size());
      Require(contents.substr(8, aggregateWriteBuffer.size()) == "GOOD");
      Require(contents.substr(12, 4) == seed.substr(12, 4)); }
    Require(sceKernelAioSubmitReadCommands(nullptr, 1, 1, &id) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioSubmitReadCommands(requests, 0, 1, &id) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioSubmitReadCommands(requests, 1, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    state = 7;
    Require(sceKernelAioWaitRequest(9999, &state, nullptr) == 0 && state == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioWaitRequest(id, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioPollRequest(id, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    for (const std::int32_t unknown : {0, -1, 511, 512, 100000}) {
        polled = 7;
        Require(sceKernelAioPollRequest(unknown, &polled) == 0 && polled == SCE_KERNEL_ERROR_ESRCH);
    }
    Require(sceKernelAioDeleteRequest(id, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    const int batchFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(batchFd >= 0);
    Require(sceKernelLseek(batchFd, 2, 0) == 2);
    std::array<char, 4> batchReadBuffer{};
    KernelAioResult batchReadSuccessResult{-1, 0};
    KernelAioResult batchReadErrorResult{-1, 0};
    KernelAioRwRequest batchReads[2] = {
        {0, batchReadBuffer.size(), batchReadBuffer.data(), &batchReadSuccessResult, batchFd},
        {-1, batchReadBuffer.size(), batchReadBuffer.data(), &batchReadErrorResult, batchFd},
    };
    std::int32_t batchReadIds[2] = {0, 0};
    Require(sceKernelAioSubmitReadCommandsMultiple(batchReads, 2, 1, batchReadIds) == 0);
    Require(batchReadIds[0] > 0 && batchReadIds[1] > 0 && batchReadIds[0] != batchReadIds[1]);
    Require(batchReadSuccessResult.state == 3 && batchReadSuccessResult.return_value == 4);
    Require(batchReadErrorResult.state == 3 && batchReadErrorResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    std::int32_t batchReadStates[2] = {0, 0};
    Require(sceKernelAioWaitRequests(batchReadIds, 2, batchReadStates, 1, nullptr) == 0);
    Require(batchReadStates[0] == 3 && batchReadStates[1] == 3);
    std::array<char, 4> batchWriteData = {'A', 'B', 'C', 'D'};
    KernelAioResult batchWriteSuccessResult{-1, 0};
    KernelAioResult batchWriteErrorResult{-1, 0};
    KernelAioRwRequest batchWrites[2] = {
        {12, batchWriteData.size(), batchWriteData.data(), &batchWriteSuccessResult, batchFd},
        {-1, batchWriteData.size(), batchWriteData.data(), &batchWriteErrorResult, batchFd},
    };
    std::int32_t batchWriteIds[2] = {0, 0};
    Require(sceKernelAioSubmitWriteCommandsMultiple(batchWrites, 2, 1, batchWriteIds) == 0);
    Require(batchWriteIds[0] > 0 && batchWriteIds[1] > 0 && batchWriteIds[0] != batchWriteIds[1]);
    Require(batchWriteSuccessResult.state == 3 && batchWriteSuccessResult.return_value == 4);
    Require(batchWriteErrorResult.state == 3 && batchWriteErrorResult.return_value == SCE_KERNEL_ERROR_EINVAL);
    std::int32_t batchWriteStates[2] = {0, 0};
    Require(sceKernelAioWaitRequests(batchWriteIds, 2, batchWriteStates, 1, nullptr) == 0);
    Require(batchWriteStates[0] == 3 && batchWriteStates[1] == 3);
    std::array<char, 4> patch = {'W', 'X', 'Y', 'Z'};
    KernelAioResult patchResult{-1, 0};
    KernelAioResult badResult{-1, 0};
    KernelAioRwRequest writeBatch[2] = {
        {8, patch.size(), patch.data(), &patchResult, batchFd},
        {0, patch.size(), patch.data(), &badResult, -1},
    };
    std::int32_t writeIds[2] = {0, 0};
    Require(sceKernelAioSubmitWriteCommandsMultiple(writeBatch, 2, 1, writeIds) == 0);
    Require(sceKernelLseek(batchFd, 0, 1) == 2);
    Require(writeIds[0] > 0 && writeIds[1] > 0 && writeIds[0] != writeIds[1]);
    Require(patchResult.state == 3 && patchResult.return_value == 4);
    Require(badResult.state == 3 && badResult.return_value == SCE_KERNEL_ERROR_EBADF);
    std::int32_t writeStates[2] = {0, 0};
    Require(sceKernelAioWaitRequests(writeIds, 2, writeStates, 1, nullptr) == 0);
    Require(writeStates[0] == 3 && writeStates[1] == 3);
    std::int32_t polledStates[2] = {0, 0};
    Require(sceKernelAioPollRequests(writeIds, 2, polledStates) == 0);
    Require(polledStates[0] == 0x10003 && polledStates[1] == 0x10003);
    std::array<char, 4> readBack{};
    std::array<char, 4> tail{};
    KernelAioResult readBackResult{-1, 0};
    KernelAioResult tailResult{-1, 0};
    KernelAioRwRequest readBatch[2] = {
        {8, readBack.size(), readBack.data(), &readBackResult, batchFd},
        {60, tail.size(), tail.data(), &tailResult, batchFd},
    };
    std::int32_t readIds[2] = {0, 0};
    Require(sceKernelAioSubmitReadCommandsMultiple(readBatch, 2, 1, readIds) == 0);
    Require(readIds[0] > 0 && readIds[1] > 0 && readIds[0] != readIds[1]);
    Require(readBackResult.state == 3 && readBackResult.return_value == 4);
    Require(std::memcmp(readBack.data(), "WXYZ", 4) == 0);
    Require(tailResult.state == 3 && tailResult.return_value == 4);
    Require(std::memcmp(tail.data(), seed.data() + 60, 4) == 0);
    std::int32_t readStates[2] = {0, 0};
    std::uint32_t timeout = 1000;
    Require(sceKernelAioWaitRequests(readIds, 2, readStates, 2, &timeout) == 0);
    Require(readStates[0] == 3 && readStates[1] == 3);
    std::int32_t cancelled = 0;
    Require(sceKernelAioCancelRequest(readIds[0], &cancelled) == 0);
    Require(cancelled == 3);
    Require(sceKernelAioPollRequest(readIds[0], &polled) == 0);
    Require(polled == 0x10003);
    Require(sceKernelAioCancelRequest(0, &cancelled) == 0);
    Require(cancelled == SCE_KERNEL_ERROR_ESRCH);
    std::int32_t cancelIds[2] = {0, readIds[1]};
    std::int32_t cancelStates[2] = {0, 0};
    Require(sceKernelAioCancelRequests(cancelIds, 2, cancelStates) == 0);
    Require(cancelStates[0] == SCE_KERNEL_ERROR_ESRCH && cancelStates[1] == 3);
    Require(sceKernelAioPollRequest(readIds[1], &polled) == 0);
    Require(polled == 0x10003);
    Require(sceKernelAioCancelRequest(readIds[1], &cancelled) == 0 && cancelled == 3);
    Require(sceKernelAioPollRequests(readIds, 2, polledStates) == 0);
    Require(polledStates[0] == 0x10003 && polledStates[1] == 0x10003);
    std::int32_t deletedRets[2] = {-1, -1};
    Require(sceKernelAioDeleteRequests(writeIds, 2, deletedRets) == 0);
    Require(deletedRets[0] == 0 && deletedRets[1] == 0);
    Require(sceKernelAioPollRequest(writeIds[0], &polled) == 0);
    Require(polled == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioDeleteRequests(writeIds, 2, deletedRets) == 0);
    Require(deletedRets[0] == SCE_KERNEL_ERROR_ESRCH && deletedRets[1] == SCE_KERNEL_ERROR_ESRCH);
    std::int32_t deletedRet = 7;
    Require(sceKernelAioDeleteRequest(writeIds[0], &deletedRet) == 0 && deletedRet == SCE_KERNEL_ERROR_ESRCH);
    std::uint32_t unknownTimeout = 1000;
    Require(sceKernelAioWaitRequests(writeIds, 2, polledStates, 1, &unknownTimeout) == 0);
    Require(polledStates[0] == SCE_KERNEL_ERROR_ESRCH && polledStates[1] == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioDeleteRequests(writeIds, 0, deletedRets) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelClose(batchFd) == 0);
    std::int32_t badIds[2] = {readIds[0], 9999};
    std::int32_t untouched[2] = {7, 7};
    Require(sceKernelAioSubmitReadCommandsMultiple(nullptr, 1, 1, readIds) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioSubmitReadCommandsMultiple(readBatch, 1, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioSubmitWriteCommandsMultiple(writeBatch, 0, 1, writeIds) == SCE_KERNEL_ERROR_EINVAL);
    KernelAioRwRequest noResult{0, 1, readBack.data(), nullptr, batchFd};
    Require(sceKernelAioSubmitWriteCommandsMultiple(&noResult, 1, 1, writeIds) == SCE_KERNEL_ERROR_EFAULT);
    std::array<KernelAioRwRequest, 129> unread{};
    std::array<std::int32_t, 129> unreadIds{};
    unreadIds.fill(7);
    for (const auto submit : {sceKernelAioSubmitReadCommandsMultiple, sceKernelAioSubmitWriteCommandsMultiple, sceKernelAioSubmitReadCommands, sceKernelAioSubmitWriteCommands}) {
        Require(submit(unread.data(), 129, 1, unreadIds.data()) == SCE_KERNEL_ERROR_EINVAL);
        for (const std::int32_t priority : {0, 4, -1}) Require(submit(unread.data(), 1, priority, unreadIds.data()) == SCE_KERNEL_ERROR_EINVAL);
        Require(unreadIds[0] == 7);
    }
    Require(sceKernelAioWaitRequests(readIds, 2, nullptr, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioWaitRequests(nullptr, 2, readStates, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioWaitRequests(readIds, -1, readStates, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioWaitRequests(badIds, 2, untouched, 1, nullptr) == 0);
    Require(untouched[0] == 0x10003 && untouched[1] == SCE_KERNEL_ERROR_ESRCH);
    untouched[0] = untouched[1] = 7;
    Require(sceKernelAioPollRequests(readIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioPollRequests(nullptr, 2, readStates) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioPollRequests(readIds, -1, readStates) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioPollRequests(badIds, 2, untouched) == 0);
    Require(untouched[0] == 0x10003 && untouched[1] == SCE_KERNEL_ERROR_ESRCH);
    untouched[0] = untouched[1] = 7;
    Require(sceKernelAioPollRequests(readIds, 0, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioWaitRequests(readIds, 0, untouched, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioCancelRequests(readIds, 0, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioDeleteRequests(readIds, 0, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Require(untouched[0] == 7 && untouched[1] == 7);
    for (const std::uint32_t mode : {0u, 3u, 4u, 0x100u, 0xffffffffu}) {
        Require(sceKernelAioWaitRequests(readIds, 2, untouched, mode, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(untouched[0] == 7 && untouched[1] == 7);
    }
    std::array<std::int32_t, 129> manyIds{};
    std::array<std::int32_t, 129> manyStates{};
    manyIds.fill(readIds[0]);
    manyStates.fill(7);
    Require(sceKernelAioDeleteRequests(manyIds.data(), 129, manyStates.data()) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioPollRequests(manyIds.data(), 129, manyStates.data()) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioWaitRequests(manyIds.data(), 129, manyStates.data(), 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioCancelRequests(manyIds.data(), 129, manyStates.data()) == SCE_KERNEL_ERROR_EINVAL);
    Require(manyStates[0] == 7);
    Require(sceKernelAioPollRequests(manyIds.data(), 128, manyStates.data()) == 0 && manyStates[127] == manyStates[0]);
    Require(sceKernelAioCancelRequest(9999, &cancelled) == 0 && cancelled == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioCancelRequest(readIds[0], nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioCancelRequests(badIds, 2, untouched) == 0);
    Require(untouched[0] == 3 && untouched[1] == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioCancelRequests(cancelIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioDeleteRequests(badIds, 2, untouched) == 0);
    Require(untouched[0] == 0 && untouched[1] == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelAioDeleteRequests(writeIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    const int faultFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(faultFd >= 0);
    std::array<char, 4> faultBuffer{};
    KernelAioResult faultResult{-1, 0};
    KernelAioRwRequest faultRequest{0, faultBuffer.size(), faultBuffer.data(), &faultResult, faultFd};
    const auto finished = [&] {
        std::int32_t requestId = 0;
        Require(sceKernelAioSubmitReadCommands(&faultRequest, 1, 1, &requestId) == 0 && faultResult.state == 3);
        return requestId;
    };
    auto* unreadable = reinterpret_cast<std::int32_t*>(0x10);
    std::int32_t kept[2] = {7, 7};
    Require(sceKernelAioPollRequests(unreadable, 0, kept) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelAioPollRequests(unreadable, 2, kept) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioCancelRequests(unreadable, 2, kept) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioDeleteRequests(unreadable, 2, kept) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelAioWaitRequests(unreadable, 2, kept, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(kept[0] == 7 && kept[1] == 7);
    void* readOnly = MapReadOnlyPage();
    const int submitFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(submitFd >= 0);
    const auto fileContents = [&] {
        std::ifstream stream(file, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    };
    const std::string beforeSubmits = fileContents();
    for (void* bad : {static_cast<void*>(unreadable), readOnly}) {
        for (const auto submit : {sceKernelAioSubmitReadCommands, sceKernelAioSubmitWriteCommands,
                                 sceKernelAioSubmitReadCommandsMultiple, sceKernelAioSubmitWriteCommandsMultiple}) {
            for (const std::int32_t size : {1, 2}) {
                std::array<char, 4> marker = {'m', 'n', 'o', 'p'};
                KernelAioResult markerResults[2] = {{-1, 0}, {-1, 0}};
                KernelAioRwRequest markerRequests[2] = {
                    {0, marker.size(), marker.data(), &markerResults[0], submitFd},
                    {16, marker.size(), marker.data(), &markerResults[1], submitFd},
                };
                Require(submit(markerRequests, size, 1, static_cast<std::int32_t*>(bad)) == SCE_KERNEL_ERROR_EFAULT);
                Require(std::string(marker.data(), marker.size()) == "mnop");
                for (const auto& markerResult : markerResults) Require(markerResult.state == 0 && markerResult.return_value == -1);
            }
        }
    }
    Require(fileContents() == beforeSubmits);
    Require(sceKernelClose(submitFd) == 0);
    for (void* bad : {static_cast<void*>(unreadable), readOnly}) {
        auto* badState = static_cast<std::int32_t*>(bad);
        const std::int32_t pollId = finished();
        Require(sceKernelAioPollRequest(pollId, badState) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioPollRequest(pollId, &polled) == 0 && polled == 0x10003);
        const std::int32_t cancelId = finished();
        Require(sceKernelAioCancelRequest(cancelId, badState) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioPollRequest(cancelId, &polled) == 0 && polled == 3);
        const std::int32_t deleteId = finished();
        Require(sceKernelAioDeleteRequest(deleteId, badState) == SCE_KERNEL_ERROR_EFAULT);
        std::int32_t removed = 7;
        Require(sceKernelAioDeleteRequest(deleteId, &removed) == 0 && removed == SCE_KERNEL_ERROR_ESRCH);
        const std::int32_t waitId = finished();
        Require(sceKernelAioWaitRequest(waitId, badState, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioPollRequest(waitId, &polled) == 0 && polled == 0x10003);
        std::int32_t waitIds[2] = {finished(), finished()};
        std::int32_t waitStates[2] = {7, 7};
        Require(sceKernelAioWaitRequests(waitIds, 2, badState, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioPollRequests(waitIds, 2, waitStates) == 0 && waitStates[0] == 0x10003 && waitStates[1] == 0x10003);
        Require(sceKernelAioDeleteRequests(waitIds, 2, waitStates) == 0 && waitStates[0] == 0 && waitStates[1] == 0);
        std::int32_t batchIds[2] = {finished(), finished()};
        std::int32_t batchStates[2] = {7, 7};
        Require(sceKernelAioPollRequests(batchIds, 2, badState) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioCancelRequests(batchIds, 2, badState) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioPollRequests(batchIds, 2, batchStates) == 0 && batchStates[0] == 0x10003 && batchStates[1] == 0x10003);
        Require(sceKernelAioDeleteRequests(batchIds, 2, badState) == SCE_KERNEL_ERROR_EFAULT);
        Require(sceKernelAioDeleteRequests(batchIds, 2, batchStates) == 0);
        Require(batchStates[0] == SCE_KERNEL_ERROR_ESRCH && batchStates[1] == SCE_KERNEL_ERROR_ESRCH);
        DeleteAio(pollId);
        DeleteAio(cancelId);
        DeleteAio(waitId);
    }
    Require(*static_cast<std::int32_t*>(readOnly) == 0);
    UnmapReadOnlyPage(readOnly);
    const std::int32_t twiceId = finished();
    std::int32_t twice[2] = {twiceId, twiceId};
    std::int32_t twiceStates[2] = {7, 7};
    Require(sceKernelAioPollRequests(twice, 2, twiceStates) == 0 && twiceStates[0] == 3 && twiceStates[1] == 0x10003);
    Require(sceKernelAioDeleteRequests(twice, 2, twiceStates) == 0 && twiceStates[0] == 0 && twiceStates[1] == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelClose(faultFd) == 0);
    const auto shared = root / "shared.bin";
    std::string sharedContents;
    for (char letter : {'A', 'B', 'C', 'D'}) sharedContents.append(1024, letter);
    { std::ofstream stream(shared, std::ios::binary); stream << sharedContents; }
    const int sharedFd = sceKernelOpen(shared.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(sharedFd >= 0);
    std::atomic<bool> misread{false};
    std::vector<std::thread> readers;
    for (std::int64_t offset = 0; offset < 4096; offset += 1024) {
        readers.emplace_back([&, offset] {
            for (int i = 0; i < 2000 && !misread; ++i) {
                std::array<char, 16> bytes{};
                KernelAioResult result{-1, 0};
                KernelAioRwRequest request{offset, bytes.size(), bytes.data(), &result, sharedFd};
                std::int32_t requestId = 0;
                if (sceKernelAioSubmitReadCommands(&request, 1, 1, &requestId) != 0 || result.state != 3 ||
                    result.return_value != 16 || std::memcmp(bytes.data(), sharedContents.data() + offset, 16) != 0) {
                    misread = true;
                }
                DeleteAio(requestId);
            }
        });
    }
    for (auto& reader : readers) reader.join();
    Require(!misread);
    Require(sceKernelClose(sharedFd) == 0);
    const auto appendFile = root / "append.bin";
    { std::ofstream stream(appendFile, std::ios::binary); stream << seed; }
    const int appendFd = sceKernelOpen(appendFile.string().c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_APPEND, 0);
    Require(appendFd >= 0);
    std::array<char, 4> appendBuffer = {'T', 'A', 'I', 'L'};
    KernelAioResult appendResult{-1, 0};
    KernelAioRwRequest appendRequest{0, appendBuffer.size(), appendBuffer.data(), &appendResult, appendFd};
    std::int32_t appendId = 0;
    Require(sceKernelAioSubmitWriteCommands(&appendRequest, 1, 1, &appendId) == 0);
    std::int32_t appendState = 0;
    Require(sceKernelAioWaitRequest(appendId, &appendState, nullptr) == 0);
    Require(appendState == 3 && appendResult.state == 3 && appendResult.return_value == 4);
    Require(sceKernelClose(appendFd) == 0);
    { std::ifstream stream(appendFile, std::ios::binary);
      std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
      Require(contents == seed + "TAIL"); }
    Require(std::filesystem::remove_all(root) > 0);
}

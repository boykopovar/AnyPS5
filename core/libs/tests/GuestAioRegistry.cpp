#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "tests/AioTest.hpp"

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Aio check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static void DeleteAio(std::int32_t id) {
    std::int32_t removed = -1;
    Check(sceKernelAioDeleteRequest(id, &removed) == 0 && removed == 0);
}

static std::int32_t SubmitEmptyAio() {
    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{0, 0, nullptr, &result, -1};
    std::int32_t id = 0;
    Check(sceKernelAioSubmitReadCommands(&request, 1, 1, &id) == 0);
    Check(result.state == 3 && result.return_value == SCE_KERNEL_ERROR_EBADF);
    return id;
}

static void CheckAioIdentity() {
    const auto retained = SubmitEmptyAio();
    std::int32_t state = 0;
    Check(sceKernelAioPollRequest(retained, &state) == 0 && state == 3);
    auto previous = retained;
    for (int iteration = 0; iteration < 1100; ++iteration) {
        const auto id = SubmitEmptyAio();
        Check(id > previous);
        previous = id;
        DeleteAio(id);
        Check(sceKernelAioPollRequest(retained, &state) == 0 && state == 0x10003);
    }
    DeleteAio(retained);
    for (int iteration = 0; iteration < 1100; ++iteration) {
        const auto id = SubmitEmptyAio();
        Check(id > previous);
        previous = id;
        DeleteAio(id);
    }
    Check(sceKernelAioPollRequest(retained, &state) == 0 && state == SCE_KERNEL_ERROR_ESRCH);
}

static void CheckAioAdmission() {
    const auto file = std::filesystem::path("anyps5-aio-admission-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    { std::ofstream stream(file, std::ios::binary); stream << 'Z'; Check(static_cast<bool>(stream)); }
    const int fd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(fd >= 0);
    std::vector<std::int32_t> retained;
    for (int index = 0; index < 510; ++index) retained.push_back(SubmitEmptyAio());
    std::array<KernelAioResult, 2> results{{{-7, 7}, {-7, 7}}};
    char byte = 'X';
    KernelAioRwRequest requests[2]{{0, 1, &byte, &results[0], fd}, {0, 1, &byte, &results[1], fd}};
    std::int32_t state = 0;
    Check(sceKernelAioPollRequest(retained[0], &state) == 0 && state == 3);
    for (const auto submit : {sceKernelAioSubmitReadCommandsMultiple, sceKernelAioSubmitWriteCommandsMultiple}) {
        std::int32_t ids[2]{-7, -7};
        bool refused = false;
        try { submit(requests, 2, 1, ids); } catch (const std::runtime_error&) { refused = true; }
        Check(refused && ids[0] == -7 && ids[1] == -7);
        Check(results[0].state == 7 && results[0].return_value == -7 && results[1].state == 7 && results[1].return_value == -7);
    }
    retained.push_back(SubmitEmptyAio());
    for (const auto submit : {sceKernelAioSubmitReadCommands, sceKernelAioSubmitWriteCommands}) {
        std::int32_t id = -7;
        bool refused = false;
        try { submit(requests, 1, 1, &id); } catch (const std::runtime_error&) { refused = true; }
        Check(refused && id == -7 && results[0].state == 7 && results[0].return_value == -7);
    }
    Check(sceKernelAioPollRequest(retained[0], &state) == 0 && state == 0x10003);
    { std::ifstream stream(file, std::ios::binary); Check(stream.get() == 'Z' && stream.get() == std::char_traits<char>::eof()); }
    for (const auto id : retained) DeleteAio(id);
    std::int32_t ids[2]{};
    Check(sceKernelAioSubmitReadCommandsMultiple(requests, 2, 1, ids) == 0);
    Check(ids[0] != ids[1] && results[0].state == 3 && results[1].state == 3);
    for (const auto id : ids) DeleteAio(id);
    Check(sceKernelClose(fd) == 0 && std::filesystem::remove(file));
}

static int g_aioSourceCalls = 0;

static void FailAioSource(std::uintptr_t, std::size_t) {
    if (++g_aioSourceCalls % 2 == 0) throw std::runtime_error("AIO source fault injection");
}

static void CheckAioRollback() {
    const auto file = std::filesystem::path("anyps5-aio-rollback-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    { std::ofstream stream(file, std::ios::binary); stream << "zz"; Check(static_cast<bool>(stream)); }
    const int fd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(fd >= 0);
    std::array<KernelAioResult, 2> results{{{-7, 7}, {-7, 7}}};
    char byte = 'A';
    KernelAioRwRequest requests[2]{{0, 1, &byte, &results[0], fd}, {1, 1, &byte, &results[1], fd}};
    for (const auto submit : {sceKernelAioSubmitWriteCommands, sceKernelAioSubmitWriteCommandsMultiple}) {
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(FailAioSource);
        for (int iteration = 0; iteration < 600; ++iteration) {
            results = {{{-7, 7}, {-7, 7}}};
            std::int32_t ids[2]{-7, -7};
            bool refused = false;
            const auto calls = g_aioSourceCalls;
            try { submit(requests, 2, 1, ids); } catch (const std::runtime_error& error) { refused = std::strcmp(error.what(), "AIO source fault injection") == 0; }
            Check(refused && ids[0] == -7 && ids[1] == -7);
            Check(g_aioSourceCalls == calls + 2 && results[0].state == 3 && results[0].return_value == 1 && results[1].state == 7);
        }
        GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    }
    Check(sceKernelClose(fd) == 0);
    { std::ifstream stream(file, std::ios::binary); Check(stream.get() == 'A' && stream.get() == 'z' && stream.get() == std::char_traits<char>::eof()); }
    Check(std::filesystem::remove(file));
    CheckAioAdmission();
}

static void CheckAioIdentityExhaustion() {
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    AioAdvanceIdentityForTest_nid_no_patch(maximum);
    std::array<KernelAioResult, 2> results{{{-7, 7}, {-7, 7}}};
    KernelAioRwRequest requests[2]{{0, 0, nullptr, &results[0], -1}, {0, 0, nullptr, &results[1], -1}};
    std::int32_t ids[2]{-7, -7};
    bool refused = false;
    try { sceKernelAioSubmitReadCommandsMultiple(requests, 2, 1, ids); } catch (const std::runtime_error&) { refused = true; }
    Check(refused && ids[0] == -7 && ids[1] == -7 && results[0].state == 7 && results[1].state == 7);
    const auto final = SubmitEmptyAio();
    Check(final == maximum);
    DeleteAio(final);
    refused = false;
    try { sceKernelAioSubmitReadCommands(requests, 1, 1, ids); } catch (const std::runtime_error&) { refused = true; }
    Check(refused && ids[0] == -7 && results[0].state == 7);
    std::int32_t state = 0;
    Check(sceKernelAioPollRequest(final, &state) == 0 && state == SCE_KERNEL_ERROR_ESRCH);
}

static std::mutex g_aioGateMutex;
static std::condition_variable g_aioGateChanged;
static bool g_aioGateEntered = false;
static bool g_aioGateReleased = false;
static bool g_aioGateFails = false;

static void HoldAioSource(std::uintptr_t, std::size_t) {
    std::unique_lock lock(g_aioGateMutex);
    g_aioGateEntered = true;
    g_aioGateChanged.notify_all();
    Check(g_aioGateChanged.wait_for(lock, std::chrono::seconds(5), [] { return g_aioGateReleased; }));
    if (g_aioGateFails) throw std::runtime_error("AIO gated transfer failure");
}

static void CheckAioProcessingOwnership(bool fail) {
    g_aioGateFails = fail;
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    AioAdvanceIdentityForTest_nid_no_patch(maximum - 2);
    const auto completed = SubmitEmptyAio();
    const auto pending = completed + 1;
    KernelAioResult result{-7, 7};
    char byte = 'x';
    KernelAioRwRequest request{0, 1, &byte, &result, -1};
    std::int32_t submittedId = -7;
    bool submitFailed = false;
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(HoldAioSource);
    std::thread submitter([&] {
        try { Check(sceKernelAioSubmitWriteCommands(&request, 1, 1, &submittedId) == 0); }
        catch (const std::runtime_error& error) { submitFailed = std::strcmp(error.what(), "AIO gated transfer failure") == 0; }
    });
    {
        std::unique_lock lock(g_aioGateMutex);
        Check(g_aioGateChanged.wait_for(lock, std::chrono::seconds(5), [] { return g_aioGateEntered; }));
    }
    std::int32_t state = -7;
    Check(sceKernelAioPollRequest(pending, &state) == 0 && state == 2);
    for (const auto operation : {sceKernelAioCancelRequest, sceKernelAioDeleteRequest}) {
        state = -7;
        bool refused = false;
        try { operation(pending, &state); } catch (const std::runtime_error&) { refused = true; }
        Check(refused && state == -7);
    }
    for (const auto operation : {sceKernelAioCancelRequests, sceKernelAioDeleteRequests}) {
        std::int32_t ids[2]{completed, pending};
        std::int32_t states[2]{-7, -7};
        bool refused = false;
        try { operation(ids, 2, states); } catch (const std::runtime_error&) { refused = true; }
        Check(refused && states[0] == -7 && states[1] == -7);
    }
    Check(sceKernelAioPollRequest(completed, &state) == 0 && state == 3);
    Check(sceKernelAioPollRequest(pending, &state) == 0 && state == 2);
    std::uint32_t timeout = 1000;
    Check(sceKernelAioWaitRequest(pending, &state, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT && state == 2);
    std::atomic<bool> waiterDone{false};
    std::int32_t waited = -7;
    std::thread waiter([&] {
        Check(sceKernelAioWaitRequest(pending, &waited, nullptr) == 0);
        waiterDone.store(true);
    });
    const auto waiterDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (AioProcessingWaitersForTest_nid_no_patch() == 0 && std::chrono::steady_clock::now() < waiterDeadline) std::this_thread::yield();
    Check(AioProcessingWaitersForTest_nid_no_patch() == 1);
    const auto concurrent = SubmitEmptyAio();
    Check(!waiterDone.load() && result.state == 7 && submittedId == -7);
    {
        std::lock_guard lock(g_aioGateMutex);
        g_aioGateReleased = true;
        g_aioGateChanged.notify_all();
    }
    submitter.join();
    waiter.join();
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    Check(waiterDone.load() && submitFailed == fail);
    Check(AioProcessingWaitersForTest_nid_no_patch() == 0);
    if (fail) {
        Check(waited == SCE_KERNEL_ERROR_ESRCH && submittedId == -7 && result.state == 7);
    } else {
        Check(waited == 3 && submittedId == pending);
        Check(result.state == 3 && result.return_value == SCE_KERNEL_ERROR_EBADF);
        DeleteAio(pending);
    }
    for (const auto id : {completed, concurrent}) DeleteAio(id);
}

int main(int argc, char** argv) {
    Check(argc == 2);
    if (argc > 1 && std::strcmp(argv[1], "identity") == 0) { CheckAioIdentity(); return 0; }
    if (argc > 1 && std::strcmp(argv[1], "admission") == 0) { CheckAioAdmission(); return 0; }
    if (argc > 1 && std::strcmp(argv[1], "rollback") == 0) { CheckAioRollback(); return 0; }
    if (argc > 1 && std::strcmp(argv[1], "identity-exhaustion") == 0) { CheckAioIdentityExhaustion(); return 0; }
    if (argc > 1 && std::strcmp(argv[1], "processing") == 0) { CheckAioProcessingOwnership(false); return 0; }
    if (argc > 1 && std::strcmp(argv[1], "processing-rollback") == 0) { CheckAioProcessingOwnership(true); return 0; }
    Check(false);
}

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
int APS5_VABI sceKernelAioDeleteRequest(std::int32_t id, std::int32_t* ret);
int APS5_VABI sceKernelAioInitializeImpl(void* param, std::int32_t size);
void APS5_VABI sceKernelAioInitializeParam(void* param);
int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, std::int32_t* id);
int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, std::int32_t* id);
int APS5_VABI sceKernelAioPollRequest(std::int32_t id, std::int32_t* state);
int APS5_VABI sceKernelAioWaitRequest(std::int32_t id, std::int32_t* state, std::uint32_t* usec);
int APS5_VABI sceKernelAioSubmitReadCommandsMultiple(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, std::int32_t* id);
int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, std::int32_t* id);
int APS5_VABI sceKernelAioWaitRequests(std::int32_t* id, std::int32_t num, std::int32_t* state, std::uint32_t mode, std::uint32_t* usec);
int APS5_VABI sceKernelAioPollRequests(std::int32_t* id, std::int32_t num, std::int32_t* state);
int APS5_VABI sceKernelAioCancelRequest(std::int32_t id, std::int32_t* state);
int APS5_VABI sceKernelAioCancelRequests(std::int32_t* id, std::int32_t num, std::int32_t* state);
int APS5_VABI sceKernelAioDeleteRequests(std::int32_t* id, std::int32_t num, std::int32_t* ret);
}
static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Aio check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)
int main() {
    const auto root = std::filesystem::path("anyps5-aio-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root));
    const auto file = root / "data.bin";
    const std::string seed = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    { std::ofstream stream(file, std::ios::binary); stream << seed; }
    Check(sceKernelAioInitializeImpl(nullptr, 0) == 0);
    int initParam = 0;
    sceKernelAioInitializeParam(&initParam);
    const int fd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    std::array<char, 16> first{};
    std::array<char, 16> second{};
    KernelAioResult firstResult{-1, 0};
    KernelAioResult secondResult{-1, 0};
    KernelAioRwRequest requests[2] = {
        {0, first.size(), first.data(), &firstResult, fd},
        {32, second.size(), second.data(), &secondResult, fd},
    };
    std::int32_t id = 0;
    Check(sceKernelAioSubmitReadCommands(requests, 2, 0, &id) == 0);
    Check(id > 0);
    std::int32_t state = 0;
    Check(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Check(state == 3);
    Check(firstResult.state == 3 && firstResult.return_value == 16);
    Check(secondResult.state == 3 && secondResult.return_value == 16);
    Check(std::memcmp(first.data(), seed.data(), 16) == 0);
    Check(std::memcmp(second.data(), seed.data() + 32, 16) == 0);
    std::int32_t polled = 0;
    Check(sceKernelAioPollRequest(id, &polled) == 0);
    Check(polled == 3);
    std::int32_t deleted = -1;
    Check(sceKernelAioDeleteRequest(id, &deleted) == 0);
    Check(deleted == 0);
    Check(sceKernelClose(fd) == 0);
    const int writeFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(writeFd >= 0);
    std::array<char, 5> written = {'H', 'E', 'L', 'L', 'O'};
    KernelAioResult writeResult{-1, 0};
    KernelAioRwRequest writeRequest{0, written.size(), written.data(), &writeResult, writeFd};
    std::int32_t writeId = 0;
    Check(sceKernelAioSubmitWriteCommands(&writeRequest, 1, 0, &writeId) == 0);
    std::int32_t writeState = 0;
    Check(sceKernelAioWaitRequest(writeId, &writeState, nullptr) == 0);
    Check(writeState == 3 && writeResult.state == 3 && writeResult.return_value == 5);
    Check(sceKernelClose(writeFd) == 0);
    { std::ifstream stream(file, std::ios::binary); std::string contents; std::getline(stream, contents);
      Check(contents.rfind("HELLO", 0) == 0); }
    Check(sceKernelAioSubmitReadCommands(nullptr, 1, 0, &id) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioSubmitReadCommands(requests, 0, 0, &id) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioSubmitReadCommands(requests, 1, 0, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioWaitRequest(9999, &state, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioWaitRequest(id, nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioPollRequest(id, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioPollRequest(0, &polled) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioDeleteRequest(id, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    const int batchFd = sceKernelOpen(file.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Check(batchFd >= 0);
    Check(sceKernelLseek(batchFd, 2, 0) == 2);
    std::array<char, 4> patch = {'W', 'X', 'Y', 'Z'};
    KernelAioResult patchResult{-1, 0};
    KernelAioResult badResult{-1, 0};
    KernelAioRwRequest writeBatch[2] = {
        {8, patch.size(), patch.data(), &patchResult, batchFd},
        {0, patch.size(), patch.data(), &badResult, -1},
    };
    std::int32_t writeIds[2] = {0, 0};
    Check(sceKernelAioSubmitWriteCommandsMultiple(writeBatch, 2, 0, writeIds) == 0);
    Check(sceKernelLseek(batchFd, 0, 1) == 2);
    Check(writeIds[0] > 0 && writeIds[1] > 0 && writeIds[0] != writeIds[1]);
    Check(patchResult.state == 3 && patchResult.return_value == 4);
    Check(badResult.state == 4 && badResult.return_value == SCE_KERNEL_ERROR_EBADF);
    std::int32_t writeStates[2] = {0, 0};
    Check(sceKernelAioWaitRequests(writeIds, 2, writeStates, 1, nullptr) == 0);
    Check(writeStates[0] == 3 && writeStates[1] == 4);
    std::int32_t polledStates[2] = {0, 0};
    Check(sceKernelAioPollRequests(writeIds, 2, polledStates) == 0);
    Check(polledStates[0] == 3 && polledStates[1] == 4);
    std::array<char, 4> readBack{};
    std::array<char, 4> tail{};
    KernelAioResult readBackResult{-1, 0};
    KernelAioResult tailResult{-1, 0};
    KernelAioRwRequest readBatch[2] = {
        {8, readBack.size(), readBack.data(), &readBackResult, batchFd},
        {60, tail.size(), tail.data(), &tailResult, batchFd},
    };
    std::int32_t readIds[2] = {0, 0};
    Check(sceKernelAioSubmitReadCommandsMultiple(readBatch, 2, 0, readIds) == 0);
    Check(readIds[0] > 0 && readIds[1] > 0 && readIds[0] != readIds[1]);
    Check(readBackResult.state == 3 && readBackResult.return_value == 4);
    Check(std::memcmp(readBack.data(), "WXYZ", 4) == 0);
    Check(tailResult.state == 3 && tailResult.return_value == 4);
    Check(std::memcmp(tail.data(), seed.data() + 60, 4) == 0);
    std::int32_t readStates[2] = {0, 0};
    std::uint32_t timeout = 1000;
    Check(sceKernelAioWaitRequests(readIds, 2, readStates, 2, &timeout) == 0);
    Check(readStates[0] == 3 && readStates[1] == 3);
    std::int32_t cancelled = 0;
    Check(sceKernelAioCancelRequest(readIds[0], &cancelled) == 0);
    Check(cancelled == 4);
    Check(sceKernelAioPollRequest(readIds[0], &polled) == 0);
    Check(polled == 4);
    Check(sceKernelAioCancelRequest(0, &cancelled) == 0);
    Check(cancelled == 2);
    std::int32_t cancelIds[2] = {0, readIds[1]};
    std::int32_t cancelStates[2] = {0, 0};
    Check(sceKernelAioCancelRequests(cancelIds, 2, cancelStates) == 0);
    Check(cancelStates[0] == 2 && cancelStates[1] == 4);
    Check(sceKernelAioPollRequest(readIds[1], &polled) == 0);
    Check(polled == 4);
    Check(sceKernelAioPollRequests(readIds, 2, polledStates) == 0);
    Check(polledStates[0] == 4 && polledStates[1] == 4);
    std::int32_t deletedRets[2] = {-1, -1};
    Check(sceKernelAioDeleteRequests(writeIds, 2, deletedRets) == 0);
    Check(deletedRets[0] == 0 && deletedRets[1] == 0);
    Check(sceKernelAioPollRequest(writeIds[0], &polled) == 0);
    Check(polled == 4);
    Check(sceKernelAioDeleteRequests(writeIds, 0, deletedRets) == 0);
    Check(sceKernelClose(batchFd) == 0);
    std::int32_t badIds[2] = {readIds[0], 9999};
    std::int32_t untouched[2] = {7, 7};
    Check(sceKernelAioSubmitReadCommandsMultiple(nullptr, 1, 0, readIds) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioSubmitReadCommandsMultiple(readBatch, 1, 0, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioSubmitWriteCommandsMultiple(writeBatch, 0, 0, writeIds) == SCE_KERNEL_ERROR_EINVAL);
    KernelAioRwRequest noResult{0, 1, readBack.data(), nullptr, batchFd};
    Check(sceKernelAioSubmitWriteCommandsMultiple(&noResult, 1, 0, writeIds) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioWaitRequests(readIds, 2, nullptr, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioWaitRequests(nullptr, 2, readStates, 1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioWaitRequests(readIds, -1, readStates, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioWaitRequests(badIds, 2, untouched, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Check(untouched[0] == 7 && untouched[1] == 7);
    Check(sceKernelAioPollRequests(readIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioPollRequests(nullptr, 2, readStates) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioPollRequests(readIds, -1, readStates) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioPollRequests(badIds, 2, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Check(untouched[0] == 7 && untouched[1] == 7);
    Check(sceKernelAioPollRequests(readIds, 0, untouched) == 0);
    Check(untouched[0] == 7 && untouched[1] == 7);
    bool threw = false;
    try { sceKernelAioWaitRequests(readIds, 2, readStates, 3, nullptr); } catch (const std::runtime_error&) { threw = true; }
    Check(threw);
    threw = false;
    std::array<std::int32_t, 129> manyIds{};
    std::array<std::int32_t, 129> manyStates{};
    manyIds.fill(readIds[0]);
    try { sceKernelAioDeleteRequests(manyIds.data(), 129, manyStates.data()); } catch (const std::runtime_error&) { threw = true; }
    Check(threw);
    threw = false;
    try { sceKernelAioPollRequests(manyIds.data(), 129, manyStates.data()); } catch (const std::runtime_error&) { threw = true; }
    Check(threw);
    Check(sceKernelAioCancelRequest(9999, &cancelled) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelAioCancelRequest(readIds[0], nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioCancelRequests(badIds, 2, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Check(untouched[0] == 7 && untouched[1] == 7);
    Check(sceKernelAioCancelRequests(cancelIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Check(sceKernelAioDeleteRequests(badIds, 2, untouched) == SCE_KERNEL_ERROR_EINVAL);
    Check(untouched[0] == 7 && untouched[1] == 7);
    Check(sceKernelAioDeleteRequests(writeIds, 2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    const auto shared = root / "shared.bin";
    std::string sharedContents;
    for (char letter : {'A', 'B', 'C', 'D'}) sharedContents.append(1024, letter);
    { std::ofstream stream(shared, std::ios::binary); stream << sharedContents; }
    const int sharedFd = sceKernelOpen(shared.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    Check(sharedFd >= 0);
    std::atomic<bool> misread{false};
    std::vector<std::thread> readers;
    for (std::int64_t offset = 0; offset < 4096; offset += 1024) {
        readers.emplace_back([&, offset] {
            for (int i = 0; i < 2000 && !misread; ++i) {
                std::array<char, 16> bytes{};
                KernelAioResult result{-1, 0};
                KernelAioRwRequest request{offset, bytes.size(), bytes.data(), &result, sharedFd};
                std::int32_t requestId = 0;
                if (sceKernelAioSubmitReadCommands(&request, 1, 0, &requestId) != 0 || result.state != 3 ||
                    result.return_value != 16 || std::memcmp(bytes.data(), sharedContents.data() + offset, 16) != 0) {
                    misread = true;
                }
            }
        });
    }
    for (auto& reader : readers) reader.join();
    Check(!misread);
    Check(sceKernelClose(sharedFd) == 0);
    Check(std::filesystem::remove_all(root) > 0);
}

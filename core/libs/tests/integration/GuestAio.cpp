#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::int32_t processing = 2;
constexpr std::int32_t completed = 3;
constexpr std::int32_t aborted = 4;
constexpr std::uint32_t resultCompleted = 3;
constexpr std::uint32_t resultAborted = 4;
constexpr std::uint32_t waitAnd = 1;
constexpr std::uint32_t waitOr = 2;
constexpr std::int32_t unknownId = 9999;
const std::string seed = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class ScratchDirectory {
public:
    ScratchDirectory() : root(UniqueName()) {
        Require(std::filesystem::create_directory(root), "create the test directory " + root.string());
    }

    ~ScratchDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    const std::filesystem::path& Path() const noexcept { return root; }

private:
    static std::filesystem::path UniqueName() {
        static std::atomic<unsigned> counter{0};
        return "anyps5-aio-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
               std::to_string(counter++);
    }

    std::filesystem::path root;
};

class DataFile {
public:
    DataFile() : file(directory.Path() / "data.bin") {
        std::ofstream stream(file, std::ios::binary);
        stream << seed;
    }

    const std::filesystem::path& Path() const noexcept { return file; }

private:
    ScratchDirectory directory;
    std::filesystem::path file;
};

class KernelFile {
public:
    KernelFile(const std::filesystem::path& path, int flags) : descriptor(sceKernelOpen(path.string().c_str(), flags, 0)) {
        Require(descriptor >= 0, "open " + path.string());
    }

    ~KernelFile() {
        if (descriptor >= 0) sceKernelClose(descriptor);
    }

    KernelFile(const KernelFile&) = delete;
    KernelFile& operator=(const KernelFile&) = delete;

    int Descriptor() const noexcept { return descriptor; }

    int Close() {
        const int result = sceKernelClose(descriptor);
        descriptor = -1;
        return result;
    }

private:
    int descriptor;
};

void DeleteRequest(std::int32_t id) {
    std::int32_t ignored = 0;
    if (id > 0) sceKernelAioDeleteRequest(id, &ignored);
}

struct TwoReads {
    TwoReads() = default;
    TwoReads(const TwoReads&) = delete;
    TwoReads& operator=(const TwoReads&) = delete;
    ~TwoReads() { DeleteRequest(id); }

    int Submit() { return sceKernelAioSubmitReadCommands(requests.data(), 2, 0, &id); }

    void SubmitCompleted() {
        RequireEqual(Submit(), 0, "submit two reads");
        std::int32_t state = 0;
        RequireEqual(sceKernelAioWaitRequest(id, &state, nullptr), 0, "wait for two reads");
    }

    DataFile data;
    KernelFile file{data.Path(), SCE_KERNEL_O_RDONLY};
    std::array<char, 16> first{};
    std::array<char, 16> second{};
    KernelAioResult firstResult{-1, 0};
    KernelAioResult secondResult{-1, 0};
    std::array<KernelAioRwRequest, 2> requests{{
        {0, first.size(), first.data(), &firstResult, file.Descriptor()},
        {32, second.size(), second.data(), &secondResult, file.Descriptor()},
    }};
    std::int32_t id = 0;
};

struct Batches {
    Batches() = default;
    Batches(const Batches&) = delete;
    Batches& operator=(const Batches&) = delete;

    ~Batches() {
        for (const auto requestId : writeIds) DeleteRequest(requestId);
        for (const auto requestId : readIds) DeleteRequest(requestId);
    }

    int SubmitWrites() { return sceKernelAioSubmitWriteCommandsMultiple(writeBatch.data(), 2, 0, writeIds.data()); }
    int SubmitReads() { return sceKernelAioSubmitReadCommandsMultiple(readBatch.data(), 2, 0, readIds.data()); }

    void SubmitBoth() {
        RequireEqual(SubmitWrites(), 0, "submit write batch");
        RequireEqual(SubmitReads(), 0, "submit read batch");
    }

    DataFile data;
    KernelFile file{data.Path(), SCE_KERNEL_O_RDWR};
    std::array<char, 4> patch = {'W', 'X', 'Y', 'Z'};
    KernelAioResult patchResult{-1, 0};
    KernelAioResult badResult{-1, 0};
    std::array<KernelAioRwRequest, 2> writeBatch{{
        {8, patch.size(), patch.data(), &patchResult, file.Descriptor()},
        {0, patch.size(), patch.data(), &badResult, -1},
    }};
    std::array<std::int32_t, 2> writeIds{0, 0};
    std::array<char, 4> readBack{};
    std::array<char, 4> tail{};
    KernelAioResult readBackResult{-1, 0};
    KernelAioResult tailResult{-1, 0};
    std::array<KernelAioRwRequest, 2> readBatch{{
        {8, readBack.size(), readBack.data(), &readBackResult, file.Descriptor()},
        {60, tail.size(), tail.data(), &tailResult, file.Descriptor()},
    }};
    std::array<std::int32_t, 2> readIds{0, 0};
};

void RequirePair(const std::array<std::int32_t, 2>& actual, std::int32_t first, std::int32_t second, const std::string& message) {
    RequireEqual(actual[0], first, message + " [0]");
    RequireEqual(actual[1], second, message + " [1]");
}

void RequireDistinctIds(const std::array<std::int32_t, 2>& ids, const std::string& message) {
    Require(ids[0] > 0 && ids[1] > 0, message + ": ids are positive");
    Require(ids[0] != ids[1], message + ": ids are distinct");
}

const Case initialize{"AioInitialize_DefaultParameters_Succeeds", [] {
    RequireEqual(sceKernelAioInitializeImpl(nullptr, 0), 0, "initialize impl");
    int initParam = 0;
    sceKernelAioInitializeParam(&initParam);
}};

const Case submitReads{"AioSubmitReadCommands_TwoRequests_CompletesAndReadsEachOffset", [] {
    TwoReads reads;
    RequireEqual(reads.Submit(), 0, "submit");
    Require(reads.id > 0, "request id is positive");
    std::int32_t state = 0;
    RequireEqual(sceKernelAioWaitRequest(reads.id, &state, nullptr), 0, "wait");
    RequireEqual(state, completed, "request state");
    RequireEqual(reads.firstResult.state, resultCompleted, "first result state");
    RequireEqual(reads.firstResult.return_value, std::int64_t{16}, "first result bytes");
    RequireEqual(reads.secondResult.state, resultCompleted, "second result state");
    RequireEqual(reads.secondResult.return_value, std::int64_t{16}, "second result bytes");
    RequireEqual(std::string(reads.first.data(), 16), seed.substr(0, 16), "first data");
    RequireEqual(std::string(reads.second.data(), 16), seed.substr(32, 16), "second data");
    RequireEqual(reads.file.Close(), 0, "close");
}};

const Case pollCompleted{"AioPollRequest_CompletedRead_ReportsCompleted", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    std::int32_t polled = 0;
    RequireEqual(sceKernelAioPollRequest(reads.id, &polled), 0, "poll");
    RequireEqual(polled, completed, "polled state");
}};

const Case deleteCompleted{"AioDeleteRequest_CompletedRead_SucceedsWithZeroReturn", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    std::int32_t deleted = -1;
    RequireEqual(sceKernelAioDeleteRequest(reads.id, &deleted), 0, "delete");
    RequireEqual(deleted, 0, "delete return");
}};

const Case submitWrite{"AioSubmitWriteCommands_OneRequest_WritesAtOffset", [] {
    const DataFile data;
    KernelFile file(data.Path(), SCE_KERNEL_O_RDWR);
    std::array<char, 5> written = {'H', 'E', 'L', 'L', 'O'};
    KernelAioResult writeResult{-1, 0};
    KernelAioRwRequest writeRequest{0, written.size(), written.data(), &writeResult, file.Descriptor()};
    std::int32_t writeId = 0;
    RequireEqual(sceKernelAioSubmitWriteCommands(&writeRequest, 1, 0, &writeId), 0, "submit");
    std::int32_t writeState = 0;
    const int waited = sceKernelAioWaitRequest(writeId, &writeState, nullptr);
    DeleteRequest(writeId);
    RequireEqual(waited, 0, "wait");
    RequireEqual(writeState, completed, "request state");
    RequireEqual(writeResult.state, resultCompleted, "result state");
    RequireEqual(writeResult.return_value, std::int64_t{5}, "result bytes");
    RequireEqual(file.Close(), 0, "close");
    std::ifstream stream(data.Path(), std::ios::binary);
    std::string contents;
    std::getline(stream, contents);
    Require(contents.rfind("HELLO", 0) == 0, "file starts with HELLO, got " + contents);
}};

const Case submitInvalid{"AioSubmitReadCommands_InvalidArguments_ReturnErrors", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    std::int32_t id = reads.id;
    RequireEqual(sceKernelAioSubmitReadCommands(nullptr, 1, 0, &id), SCE_KERNEL_ERROR_EFAULT, "null requests");
    RequireEqual(sceKernelAioSubmitReadCommands(reads.requests.data(), 0, 0, &id), SCE_KERNEL_ERROR_EINVAL, "zero size");
    RequireEqual(sceKernelAioSubmitReadCommands(reads.requests.data(), 1, 0, nullptr), SCE_KERNEL_ERROR_EFAULT, "null id");
}};

const Case waitInvalid{"AioWaitRequest_InvalidArguments_ReturnErrors", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    std::int32_t state = 0;
    RequireEqual(sceKernelAioWaitRequest(unknownId, &state, nullptr), SCE_KERNEL_ERROR_EINVAL, "unknown id");
    RequireEqual(sceKernelAioWaitRequest(reads.id, nullptr, nullptr), SCE_KERNEL_ERROR_EFAULT, "null state");
}};

const Case pollInvalid{"AioPollRequest_InvalidArguments_ReturnErrors", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    std::int32_t polled = 0;
    RequireEqual(sceKernelAioPollRequest(reads.id, nullptr), SCE_KERNEL_ERROR_EFAULT, "null state");
    RequireEqual(sceKernelAioPollRequest(0, &polled), SCE_KERNEL_ERROR_EINVAL, "id 0");
}};

const Case deleteInvalid{"AioDeleteRequest_NullReturn_FailsWithEfault", [] {
    TwoReads reads;
    reads.SubmitCompleted();
    RequireEqual(sceKernelAioDeleteRequest(reads.id, nullptr), SCE_KERNEL_ERROR_EFAULT, "null return");
}};

const Case writeMultiple{"AioSubmitWriteCommandsMultiple_ValidAndBadDescriptor_CompletesAndAbortsEachRequest", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    RequireDistinctIds(batches.writeIds, "write ids");
    RequireEqual(batches.patchResult.state, resultCompleted, "patch result state");
    RequireEqual(batches.patchResult.return_value, std::int64_t{4}, "patch result bytes");
    RequireEqual(batches.badResult.state, resultAborted, "bad descriptor result state");
    RequireEqual(batches.badResult.return_value, static_cast<std::int64_t>(SCE_KERNEL_ERROR_EBADF), "bad descriptor result");
    RequireEqual(batches.file.Close(), 0, "close");
}};

const Case writeMultipleOffset{"AioSubmitWriteCommandsMultiple_PositionedWrites_KeepFileOffset", [] {
    Batches batches;
    RequireEqual(sceKernelLseek(batches.file.Descriptor(), 2, 0), std::int64_t{2}, "seek to 2");
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    RequireEqual(sceKernelLseek(batches.file.Descriptor(), 0, 1), std::int64_t{2}, "offset after submit");
}};

const Case waitAndMode{"AioWaitRequests_AndMode_ReportsEachState", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    std::array<std::int32_t, 2> states{0, 0};
    RequireEqual(sceKernelAioWaitRequests(batches.writeIds.data(), 2, states.data(), waitAnd, nullptr), 0, "wait");
    RequirePair(states, completed, aborted, "states");
}};

const Case pollMultiple{"AioPollRequests_CompletedAndAbortedRequests_ReportsEachState", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    std::array<std::int32_t, 2> states{0, 0};
    RequireEqual(sceKernelAioPollRequests(batches.writeIds.data(), 2, states.data()), 0, "poll");
    RequirePair(states, completed, aborted, "states");
}};

const Case readMultiple{"AioSubmitReadCommandsMultiple_AfterPatch_ReadsPatchAndTail", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit writes");
    RequireEqual(batches.SubmitReads(), 0, "submit reads");
    RequireDistinctIds(batches.readIds, "read ids");
    RequireEqual(batches.readBackResult.state, resultCompleted, "read back result state");
    RequireEqual(batches.readBackResult.return_value, std::int64_t{4}, "read back result bytes");
    RequireEqual(std::string(batches.readBack.data(), 4), std::string("WXYZ"), "read back data");
    RequireEqual(batches.tailResult.state, resultCompleted, "tail result state");
    RequireEqual(batches.tailResult.return_value, std::int64_t{4}, "tail result bytes");
    RequireEqual(std::string(batches.tail.data(), 4), seed.substr(60, 4), "tail data");
}};

const Case waitOrMode{"AioWaitRequests_OrModeWithTimeout_ReportsCompleted", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> states{0, 0};
    std::uint32_t timeout = 1000;
    RequireEqual(sceKernelAioWaitRequests(batches.readIds.data(), 2, states.data(), waitOr, &timeout), 0, "wait");
    RequirePair(states, completed, completed, "states");
}};

const Case cancelCompleted{"AioCancelRequest_CompletedRequest_ReportsAbortedAndPollAgrees", [] {
    Batches batches;
    batches.SubmitBoth();
    std::int32_t cancelled = 0;
    RequireEqual(sceKernelAioCancelRequest(batches.readIds[0], &cancelled), 0, "cancel");
    RequireEqual(cancelled, aborted, "cancelled state");
    std::int32_t polled = 0;
    RequireEqual(sceKernelAioPollRequest(batches.readIds[0], &polled), 0, "poll");
    RequireEqual(polled, aborted, "polled state");
}};

const Case cancelZero{"AioCancelRequest_IdZero_ReportsProcessing", [] {
    std::int32_t cancelled = 0;
    RequireEqual(sceKernelAioCancelRequest(0, &cancelled), 0, "cancel id 0");
    RequireEqual(cancelled, processing, "cancelled state");
}};

const Case cancelMultiple{"AioCancelRequests_ZeroAndCompletedId_ReportsProcessingAndAborted", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> cancelIds{0, batches.readIds[1]};
    std::array<std::int32_t, 2> cancelStates{0, 0};
    RequireEqual(sceKernelAioCancelRequests(cancelIds.data(), 2, cancelStates.data()), 0, "cancel");
    RequirePair(cancelStates, processing, aborted, "cancel states");
    std::int32_t polled = 0;
    RequireEqual(sceKernelAioPollRequest(batches.readIds[1], &polled), 0, "poll");
    RequireEqual(polled, aborted, "polled state");
}};

const Case pollCancelled{"AioPollRequests_AllRequestsCancelled_ReportsAborted", [] {
    Batches batches;
    batches.SubmitBoth();
    std::int32_t cancelled = 0;
    RequireEqual(sceKernelAioCancelRequest(batches.readIds[0], &cancelled), 0, "cancel first");
    std::array<std::int32_t, 2> cancelIds{0, batches.readIds[1]};
    std::array<std::int32_t, 2> cancelStates{0, 0};
    RequireEqual(sceKernelAioCancelRequests(cancelIds.data(), 2, cancelStates.data()), 0, "cancel second");
    std::array<std::int32_t, 2> states{0, 0};
    RequireEqual(sceKernelAioPollRequests(batches.readIds.data(), 2, states.data()), 0, "poll");
    RequirePair(states, aborted, aborted, "polled states");
}};

const Case deleteMultiple{"AioDeleteRequests_TwoIds_SucceedsAndAbortsRequests", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    std::array<std::int32_t, 2> deletedRets{-1, -1};
    RequireEqual(sceKernelAioDeleteRequests(batches.writeIds.data(), 2, deletedRets.data()), 0, "delete");
    RequirePair(deletedRets, 0, 0, "delete returns");
    std::int32_t polled = 0;
    RequireEqual(sceKernelAioPollRequest(batches.writeIds[0], &polled), 0, "poll");
    RequireEqual(polled, aborted, "polled state");
}};

const Case deleteZeroCount{"AioDeleteRequests_ZeroCount_Succeeds", [] {
    Batches batches;
    RequireEqual(batches.SubmitWrites(), 0, "submit");
    std::array<std::int32_t, 2> deletedRets{-1, -1};
    RequireEqual(sceKernelAioDeleteRequests(batches.writeIds.data(), 0, deletedRets.data()), 0, "delete zero ids");
}};

const Case submitMultipleInvalid{"AioSubmitCommandsMultiple_InvalidArguments_ReturnErrors", [] {
    Batches batches;
    RequireEqual(sceKernelAioSubmitReadCommandsMultiple(nullptr, 1, 0, batches.readIds.data()), SCE_KERNEL_ERROR_EFAULT,
                 "read null requests");
    RequireEqual(sceKernelAioSubmitReadCommandsMultiple(batches.readBatch.data(), 1, 0, nullptr), SCE_KERNEL_ERROR_EFAULT,
                 "read null ids");
    RequireEqual(sceKernelAioSubmitWriteCommandsMultiple(batches.writeBatch.data(), 0, 0, batches.writeIds.data()),
                 SCE_KERNEL_ERROR_EINVAL, "write zero size");
    KernelAioRwRequest noResult{0, 1, batches.readBack.data(), nullptr, batches.file.Descriptor()};
    RequireEqual(sceKernelAioSubmitWriteCommandsMultiple(&noResult, 1, 0, batches.writeIds.data()), SCE_KERNEL_ERROR_EFAULT,
                 "write null result");
}};

const Case waitMultipleInvalid{"AioWaitRequests_InvalidArguments_ReturnErrorsWithoutWritingStates", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> states{0, 0};
    std::array<std::int32_t, 2> badIds{batches.readIds[0], unknownId};
    std::array<std::int32_t, 2> untouched{7, 7};
    RequireEqual(sceKernelAioWaitRequests(batches.readIds.data(), 2, nullptr, waitAnd, nullptr), SCE_KERNEL_ERROR_EFAULT,
                 "null states");
    RequireEqual(sceKernelAioWaitRequests(nullptr, 2, states.data(), waitAnd, nullptr), SCE_KERNEL_ERROR_EFAULT, "null ids");
    RequireEqual(sceKernelAioWaitRequests(batches.readIds.data(), -1, states.data(), waitAnd, nullptr), SCE_KERNEL_ERROR_EINVAL,
                 "negative count");
    RequireEqual(sceKernelAioWaitRequests(badIds.data(), 2, untouched.data(), waitAnd, nullptr), SCE_KERNEL_ERROR_EINVAL,
                 "unknown id");
    RequirePair(untouched, 7, 7, "states after unknown id");
}};

const Case pollMultipleInvalid{"AioPollRequests_InvalidArguments_ReturnErrorsWithoutWritingStates", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> states{0, 0};
    std::array<std::int32_t, 2> badIds{batches.readIds[0], unknownId};
    std::array<std::int32_t, 2> untouched{7, 7};
    RequireEqual(sceKernelAioPollRequests(batches.readIds.data(), 2, nullptr), SCE_KERNEL_ERROR_EFAULT, "null states");
    RequireEqual(sceKernelAioPollRequests(nullptr, 2, states.data()), SCE_KERNEL_ERROR_EFAULT, "null ids");
    RequireEqual(sceKernelAioPollRequests(batches.readIds.data(), -1, states.data()), SCE_KERNEL_ERROR_EINVAL, "negative count");
    RequireEqual(sceKernelAioPollRequests(badIds.data(), 2, untouched.data()), SCE_KERNEL_ERROR_EINVAL, "unknown id");
    RequirePair(untouched, 7, 7, "states after unknown id");
}};

const Case pollZeroCount{"AioPollRequests_ZeroCount_SucceedsWithoutWritingStates", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> untouched{7, 7};
    RequireEqual(sceKernelAioPollRequests(batches.readIds.data(), 0, untouched.data()), 0, "poll zero ids");
    RequirePair(untouched, 7, 7, "states");
}};

const Case waitUnknownMode{"AioWaitRequests_UnsupportedMode_Throws", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> states{0, 0};
    RequireThrows<std::runtime_error>([&] { sceKernelAioWaitRequests(batches.readIds.data(), 2, states.data(), 3, nullptr); },
                                      "wait mode 3");
}};

const Case deleteTooMany{"AioDeleteRequests_MoreThan128Ids_Throws", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 129> manyIds{};
    std::array<std::int32_t, 129> manyStates{};
    manyIds.fill(batches.readIds[0]);
    RequireThrows<std::runtime_error>([&] { sceKernelAioDeleteRequests(manyIds.data(), 129, manyStates.data()); }, "129 ids");
}};

const Case pollTooMany{"AioPollRequests_MoreThan128Ids_Throws", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 129> manyIds{};
    std::array<std::int32_t, 129> manyStates{};
    manyIds.fill(batches.readIds[0]);
    RequireThrows<std::runtime_error>([&] { sceKernelAioPollRequests(manyIds.data(), 129, manyStates.data()); }, "129 ids");
}};

const Case cancelInvalid{"AioCancelRequest_InvalidArguments_ReturnErrors", [] {
    Batches batches;
    batches.SubmitBoth();
    std::int32_t cancelled = 0;
    RequireEqual(sceKernelAioCancelRequest(unknownId, &cancelled), SCE_KERNEL_ERROR_EINVAL, "unknown id");
    RequireEqual(sceKernelAioCancelRequest(batches.readIds[0], nullptr), SCE_KERNEL_ERROR_EFAULT, "null state");
}};

const Case cancelMultipleInvalid{"AioCancelRequests_InvalidArguments_ReturnErrorsWithoutWritingStates", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> badIds{batches.readIds[0], unknownId};
    std::array<std::int32_t, 2> untouched{7, 7};
    std::array<std::int32_t, 2> cancelIds{0, batches.readIds[1]};
    RequireEqual(sceKernelAioCancelRequests(badIds.data(), 2, untouched.data()), SCE_KERNEL_ERROR_EINVAL, "unknown id");
    RequirePair(untouched, 7, 7, "states after unknown id");
    RequireEqual(sceKernelAioCancelRequests(cancelIds.data(), 2, nullptr), SCE_KERNEL_ERROR_EFAULT, "null states");
}};

const Case deleteMultipleInvalid{"AioDeleteRequests_InvalidArguments_ReturnErrorsWithoutWritingReturns", [] {
    Batches batches;
    batches.SubmitBoth();
    std::array<std::int32_t, 2> badIds{batches.readIds[0], unknownId};
    std::array<std::int32_t, 2> untouched{7, 7};
    RequireEqual(sceKernelAioDeleteRequests(badIds.data(), 2, untouched.data()), SCE_KERNEL_ERROR_EINVAL, "unknown id");
    RequirePair(untouched, 7, 7, "returns after unknown id");
    RequireEqual(sceKernelAioDeleteRequests(batches.writeIds.data(), 2, nullptr), SCE_KERNEL_ERROR_EFAULT, "null returns");
}};

const Case concurrentReads{"AioSubmitReadCommands_ConcurrentReadersOnSharedDescriptor_ReadTheirOwnOffsets", [] {
    const ScratchDirectory directory;
    const auto shared = directory.Path() / "shared.bin";
    std::string sharedContents;
    for (char letter : {'A', 'B', 'C', 'D'}) sharedContents.append(1024, letter);
    {
        std::ofstream stream(shared, std::ios::binary);
        stream << sharedContents;
    }
    KernelFile file(shared, SCE_KERNEL_O_RDONLY);
    const int sharedFd = file.Descriptor();
    std::atomic<bool> misread{false};
    std::atomic<std::int64_t> misreadOffset{-1};
    {
        std::vector<std::jthread> readers;
        for (std::int64_t offset = 0; offset < 4096; offset += 1024) {
            readers.emplace_back([&, offset] {
                for (int iteration = 0; iteration < 2000 && !misread; ++iteration) {
                    std::array<char, 16> bytes{};
                    KernelAioResult result{-1, 0};
                    KernelAioRwRequest request{offset, bytes.size(), bytes.data(), &result, sharedFd};
                    std::int32_t requestId = 0;
                    if (sceKernelAioSubmitReadCommands(&request, 1, 0, &requestId) != 0 || result.state != resultCompleted ||
                        result.return_value != 16 || std::memcmp(bytes.data(), sharedContents.data() + offset, 16) != 0) {
                        misreadOffset = offset;
                        misread = true;
                    }
                }
            });
        }
    }
    Require(!misread, "a reader misread at offset " + std::to_string(misreadOffset.load()));
    RequireEqual(file.Close(), 0, "close");
}};

} // namespace

#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "guest_fios2 check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(condition) Require((condition), __LINE__)

static std::atomic<int> g_completed{0};
static std::atomic<int> g_deleted{0};
static std::atomic<int> g_lastError{0};
static std::atomic<int> g_lastOp{0};

static int APS5_VABI CountEvents(void* context, SceFiosOp op, SceFiosOpEvent event, int error) {
    REQUIRE(context == reinterpret_cast<void*>(0x1234));
    g_lastOp = op;
    if (event == SCE_FIOS_OPEVENT_COMPLETE) {
        g_lastError = error;
        ++g_completed;
    } else if (event == SCE_FIOS_OPEVENT_DELETE) {
        ++g_deleted;
    }
    return 0;
}

static SceFiosOpAttr CountingAttr() {
    SceFiosOpAttr attr{};
    attr.pCallback = CountEvents;
    attr.pCallbackContext = reinterpret_cast<void*>(0x1234);
    return attr;
}

static void CheckInitialization() {
    REQUIRE(!sceFiosIsInitialized(nullptr));
    std::uint32_t params[16] = {};
    params[0] = (sizeof(params) << 1) | 0u;
    params[3] = 0xfeedbeef;
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(params)) == SCE_FIOS_OK);
    REQUIRE(sceFiosIsInitialized(nullptr));
    std::uint32_t copy[16] = {};
    REQUIRE(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(copy)));
    REQUIRE(copy[3] == 0xfeedbeef);
    params[3] = 0x1;
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(params));
    REQUIRE(sceFiosIsInitialized(reinterpret_cast<SceFiosParams*>(copy)) && copy[3] == 0x1);
}

static void CheckQueries(const std::string& directory, const std::string& file) {
    REQUIRE(sceFiosExistsSync(nullptr, file.c_str()));
    REQUIRE(sceFiosExistsSync(nullptr, directory.c_str()));
    REQUIRE(!sceFiosExistsSync(nullptr, (directory + "/missing").c_str()));
    REQUIRE(sceFiosFileExistsSync(nullptr, file.c_str()));
    REQUIRE(!sceFiosFileExistsSync(nullptr, directory.c_str()));
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, directory.c_str()));
    REQUIRE(!sceFiosDirectoryExistsSync(nullptr, file.c_str()));
    REQUIRE(!sceFiosExistsSync(nullptr, nullptr));
    REQUIRE(!sceFiosExistsSync(nullptr, ""));

    REQUIRE(sceFiosFileGetSizeSync(nullptr, file.c_str()) == 10);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, directory.c_str()) == SCE_FIOS_ERROR_NOT_A_FILE);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, (directory + "/missing").c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, "") == SCE_FIOS_ERROR_BAD_PATH);
    const std::string tooLong(2000, 'a');
    REQUIRE(sceFiosFileGetSizeSync(nullptr, tooLong.c_str()) == SCE_FIOS_ERROR_PATH_TOO_LONG);

    SceFiosStat stat{};
    std::memset(&stat, 0xaa, sizeof(stat));
    REQUIRE(sceFiosStatSync(nullptr, file.c_str(), &stat) == SCE_FIOS_OK);
    REQUIRE(stat.fileSize == 10 && (stat.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0);
    REQUIRE(stat.modificationDate > 1000000000ull * 1000000000ull);
    REQUIRE((stat.mode & 0170000) == 0100000);
    REQUIRE(sceFiosStatSync(nullptr, directory.c_str(), &stat) == SCE_FIOS_OK);
    REQUIRE((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
    REQUIRE(sceFiosStatSync(nullptr, (directory + "/missing").c_str(), &stat) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosStatSync(nullptr, file.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
}

static void CheckOperationLifecycle(const std::string& file) {
    const SceFiosOp op = sceFiosFileGetSize(nullptr, file.c_str());
    REQUIRE(op > 0);
    REQUIRE(sceFiosIsValidHandle(op));
    REQUIRE(sceFiosOpIsDone(op));
    REQUIRE(sceFiosOpGetError(op) == SCE_FIOS_OK);
    REQUIRE(sceFiosOpWait(op) == SCE_FIOS_OK);
    REQUIRE(sceFiosOpGetActualCount(op) == 10);
    REQUIRE(sceFiosOpGetPath(op) != nullptr && file == sceFiosOpGetPath(op));
    REQUIRE(sceFiosOpGetAttr(op) != nullptr && !sceFiosOpIsCancelled(op));
    REQUIRE(sceFiosOpGetOffset(op) == 0);
    std::vector<SceFiosOp> listed(8);
    REQUIRE(sceFiosGetAllOps(listed.data(), static_cast<std::uint32_t>(listed.size())) >= 1);
    REQUIRE(listed[0] == op);
    REQUIRE(sceFiosOpSyncWaitForIO(op) == 10);
    REQUIRE(!sceFiosIsValidHandle(op));
    REQUIRE(sceFiosOpGetError(op) == SCE_FIOS_ERROR_BAD_OP);
    REQUIRE(sceFiosOpWait(op) == SCE_FIOS_ERROR_BAD_OP);
    REQUIRE(sceFiosOpSyncWait(op) == SCE_FIOS_ERROR_BAD_OP);
    REQUIRE(sceFiosOpGetActualCount(op) == SCE_FIOS_ERROR_BAD_OP);
    REQUIRE(sceFiosOpGetPath(op) == nullptr);
    sceFiosOpDelete(op);

    bool exists = false;
    const SceFiosOp existsOp = sceFiosExists(nullptr, file.c_str(), &exists);
    REQUIRE(exists && sceFiosOpGetActualCount(existsOp) == 1);
    REQUIRE(sceFiosOpSyncWait(existsOp) == 1);
    const SceFiosOp missingOp = sceFiosExists(nullptr, "fios2_probe/none", &exists);
    REQUIRE(!exists && sceFiosOpSyncWait(missingOp) == 0);

    const SceFiosOp failed = sceFiosFileGetSize(nullptr, "fios2_probe/none");
    REQUIRE(sceFiosOpGetError(failed) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosOpSyncWait(failed) == SCE_FIOS_ERROR_BAD_PATH);
}

static void CheckCallbacks(const std::string& file) {
    g_completed = 0;
    g_deleted = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp op = sceFiosFileExists(&attr, file.c_str(), nullptr);
    REQUIRE(g_completed == 1 && g_lastOp == op && g_lastError == SCE_FIOS_OK);
    REQUIRE(g_deleted == 0);
    sceFiosOpDelete(op);
    REQUIRE(g_deleted == 1);
    REQUIRE(sceFiosFileExistsSync(&attr, file.c_str()));
    REQUIRE(g_completed == 2 && g_deleted == 2);

    const SceFiosOp failed = sceFiosFileGetSize(&attr, "fios2_probe/none");
    REQUIRE(g_completed == 3 && g_lastError == SCE_FIOS_ERROR_BAD_PATH);
    sceFiosOpDelete(failed);

    SceFiosOpAttr stored{};
    REQUIRE(!sceFiosGetGlobalDefaultOpAttr(&stored));
    SceFiosOpAttr global = CountingAttr();
    global.userTag = 77;
    sceFiosSetGlobalDefaultOpAttr(&global);
    REQUIRE(sceFiosGetGlobalDefaultOpAttr(&stored) && stored.userTag == 77);
    SceFiosOpAttr effective{};
    sceFiosGetDefaultOpAttr(&effective);
    REQUIRE(effective.userTag == 77);
    const SceFiosOp usesGlobal = sceFiosFileExists(nullptr, file.c_str(), nullptr);
    REQUIRE(sceFiosOpGetAttr(usesGlobal)->userTag == 77 && g_completed == 4);
    sceFiosOpDelete(usesGlobal);

    SceFiosOpAttr thread = CountingAttr();
    thread.userTag = 88;
    sceFiosSetThreadDefaultOpAttr(&thread);
    REQUIRE(sceFiosGetThreadDefaultOpAttr(&stored) && stored.userTag == 88);
    std::thread other([&] {
        SceFiosOpAttr seen{};
        REQUIRE(!sceFiosGetThreadDefaultOpAttr(&seen));
        sceFiosGetDefaultOpAttr(&seen);
        REQUIRE(seen.userTag == 77);
    });
    other.join();
    const SceFiosOp usesThread = sceFiosFileExists(nullptr, file.c_str(), nullptr);
    REQUIRE(sceFiosOpGetAttr(usesThread)->userTag == 88);
    sceFiosOpDelete(usesThread);
    sceFiosSetThreadDefaultOpAttr(nullptr);
    REQUIRE(!sceFiosGetThreadDefaultOpAttr(&stored));
    sceFiosSetGlobalDefaultOpAttr(nullptr);
    REQUIRE(!sceFiosGetGlobalDefaultOpAttr(&stored));
}

static void CheckSuspension(const std::string& file) {
    REQUIRE(!sceFiosIsSuspended() && sceFiosGetSuspendCount() == 0 && sceFiosIsIdle());
    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    sceFiosSuspend();
    sceFiosSuspend();
    REQUIRE(sceFiosIsSuspended() && sceFiosGetSuspendCount() == 2);
    const SceFiosOp first = sceFiosFileGetSize(&attr, file.c_str());
    const SceFiosOp second = sceFiosFileGetSize(&attr, file.c_str());
    REQUIRE(!sceFiosOpIsDone(first) && !sceFiosOpIsDone(second) && !sceFiosIsIdle());
    REQUIRE(sceFiosOpGetError(first) == 1 && g_completed == 0);
    sceFiosResume();
    REQUIRE(!sceFiosOpIsDone(first) && sceFiosGetSuspendCount() == 1);
    std::thread resumer([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sceFiosResume();
    });
    REQUIRE(sceFiosOpWait(first) == SCE_FIOS_OK);
    resumer.join();
    REQUIRE(sceFiosOpIsDone(second) && sceFiosIsIdle() && g_completed == 2);
    REQUIRE(sceFiosOpGetActualCount(first) == 10 && sceFiosOpGetActualCount(second) == 10);
    sceFiosOpDelete(first);
    sceFiosOpDelete(second);
    sceFiosResume();
    REQUIRE(sceFiosGetSuspendCount() == 0);

    sceFiosSuspend();
    const SceFiosOp cancelled = sceFiosFileGetSize(&attr, file.c_str());
    const SceFiosOp alsoCancelled = sceFiosFileGetSize(&attr, file.c_str());
    sceFiosOpCancel(cancelled);
    REQUIRE(sceFiosOpIsDone(cancelled) && sceFiosOpIsCancelled(cancelled));
    REQUIRE(sceFiosOpGetError(cancelled) == SCE_FIOS_ERROR_CANCELLED);
    REQUIRE(g_lastError == SCE_FIOS_ERROR_CANCELLED);
    sceFiosCancelAllOps();
    REQUIRE(sceFiosOpGetError(alsoCancelled) == SCE_FIOS_ERROR_CANCELLED && sceFiosIsIdle());
    sceFiosResume();
    REQUIRE(sceFiosOpSyncWaitForIO(cancelled) == SCE_FIOS_ERROR_CANCELLED);
    sceFiosOpDelete(alsoCancelled);

    sceFiosSuspend();
    const SceFiosOp abandoned = sceFiosFileGetSize(nullptr, file.c_str());
    sceFiosOpDelete(abandoned);
    REQUIRE(!sceFiosIsValidHandle(abandoned));
    sceFiosResume();
    sceFiosResume();
    REQUIRE(sceFiosGetSuspendCount() == 0);
}

static void CheckMutations(const std::string& directory) {
    const std::string sub = directory + "/sub";
    const std::string target = sub + "/a.bin";
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_ALREADY_EXISTS);
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, (directory + "/none/deeper").c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosDirectoryExistsSync(nullptr, sub.c_str()));

    REQUIRE(sceFiosFileWriteSync(nullptr, target.c_str(), "hello world", 11, 0) == 11);
    char buffer[32] = {};
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 11, 0) == 11 && std::memcmp(buffer, "hello world", 11) == 0);
    std::memset(buffer, 0, sizeof(buffer));
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 5, 6) == 5 && std::memcmp(buffer, "world", 5) == 0);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 100, 6) == 5);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 4, 500) == 0);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 0, 0) == 0);
    REQUIRE(sceFiosFileReadSync(nullptr, (sub + "/none").c_str(), buffer, 4, 0) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, -1, 0) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), buffer, 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFileReadSync(nullptr, target.c_str(), nullptr, 4, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileReadSync(nullptr, sub.c_str(), buffer, 4, 0) < 0);
    REQUIRE(sceFiosFileWriteSync(nullptr, target.c_str(), "xyzzy", 5, 20) == 5);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, target.c_str()) == 25);
    REQUIRE(sceFiosFileWriteSync(nullptr, target.c_str(), nullptr, 5, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileWriteSync(nullptr, (sub + "/none/x").c_str(), "a", 1, 0) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFileWriteSync(nullptr, (sub + "/created.bin").c_str(), "a", 1, 0) == 1);
    REQUIRE(sceFiosFileDeleteSync(nullptr, (sub + "/created.bin").c_str()) == SCE_FIOS_OK);

    std::memset(buffer, 0, sizeof(buffer));
    const SceFiosOp read = sceFiosFileRead(nullptr, target.c_str(), buffer, 5, 6);
    REQUIRE(sceFiosOpGetRequestCount(read) == 5 && sceFiosOpGetOffset(read) == 6 && sceFiosOpGetBuffer(read) == buffer);
    REQUIRE(sceFiosOpGetActualCount(read) == 5 && sceFiosOpSyncWaitForIO(read) == 5 && std::memcmp(buffer, "world", 5) == 0);

    REQUIRE(sceFiosFileTruncateSync(nullptr, target.c_str(), 4) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, target.c_str()) == 4);
    REQUIRE(sceFiosFileTruncateSync(nullptr, target.c_str(), -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFileTruncateSync(nullptr, (sub + "/none").c_str(), 4) == SCE_FIOS_ERROR_BAD_PATH);

    const std::string renamed = sub + "/b.bin";
    REQUIRE(sceFiosRenameSync(nullptr, target.c_str(), renamed.c_str()) == SCE_FIOS_OK);
    REQUIRE(!sceFiosExistsSync(nullptr, target.c_str()) && sceFiosFileExistsSync(nullptr, renamed.c_str()));
    REQUIRE(sceFiosRenameSync(nullptr, target.c_str(), renamed.c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosRenameSync(nullptr, renamed.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosRenameSync(nullptr, nullptr, renamed.c_str()) == SCE_FIOS_ERROR_BAD_PTR);

    REQUIRE(sceFiosFileDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_NOT_A_FILE);
    REQUIRE(sceFiosDirectoryDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    REQUIRE(sceFiosDirectoryDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_ERROR_ACCESS);
    REQUIRE(sceFiosFileDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileDeleteSync(nullptr, renamed.c_str()) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosDirectoryDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, sub.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileWriteSync(nullptr, renamed.c_str(), "a", 1, 0) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFileWriteSync(nullptr, (directory + "/c.bin").c_str(), "a", 1, 0) == 1);
    REQUIRE(sceFiosDeleteSync(nullptr, (directory + "/c.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, (directory + "/c.bin").c_str()) == SCE_FIOS_ERROR_BAD_PATH);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp created = sceFiosDirectoryCreate(&attr, sub.c_str());
    REQUIRE(g_completed == 1 && sceFiosOpGetError(created) == SCE_FIOS_OK);
    sceFiosOpDelete(created);
    const SceFiosOp removed = sceFiosDelete(&attr, sub.c_str());
    REQUIRE(g_completed == 2 && sceFiosOpSyncWait(removed) == SCE_FIOS_OK);
}

static void CheckFileHandles(const std::string& directory) {
    const std::string path = directory + "/handle.bin";
    SceFiosOpenParams writer{};
    writer.openFlags = (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT);
    SceFiosFH fh = 0;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosFHOpenSync(nullptr, nullptr, path.c_str(), &writer) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, nullptr, &writer) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, directory.c_str(), nullptr) == SCE_FIOS_ERROR_NOT_A_FILE);
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &writer) == SCE_FIOS_OK);
    REQUIRE(fh > 0 && sceFiosIsValidHandle(fh));
    REQUIRE(path == sceFiosFHGetPath(fh) && sceFiosFHGetOpenParams(fh)->openFlags == writer.openFlags);
    REQUIRE(sceFiosFHGetSize(fh) == 0);
    std::uint32_t listed[4] = {};
    REQUIRE(sceFiosGetAllFHs(reinterpret_cast<SceFiosFH*>(listed), 4) == 1 && listed[0] == static_cast<std::uint32_t>(fh));

    REQUIRE(sceFiosFHWriteSync(nullptr, fh, "abcdef", 6) == 6);
    REQUIRE(sceFiosFHTell(fh) == 6 && sceFiosFHGetSize(fh) == 6);
    REQUIRE(sceFiosFHSeek(fh, 1, SCE_FIOS_SEEK_SET) == 1);
    char buffer[16] = {};
    REQUIRE(sceFiosFHReadSync(nullptr, fh, buffer, 3) == 3 && std::memcmp(buffer, "bcd", 3) == 0);
    REQUIRE(sceFiosFHSeek(fh, -1, SCE_FIOS_SEEK_END) == 5 && sceFiosFHSeek(fh, 1, SCE_FIOS_SEEK_CUR) == 6);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, buffer, 3) == 0);
    REQUIRE(sceFiosFHSeek(fh, 0, 7) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHSeek(fh, -100, SCE_FIOS_SEEK_SET) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHPwriteSync(nullptr, fh, "XY", 2, 1) == 2 && sceFiosFHTell(fh) == 6);
    std::memset(buffer, 0, sizeof(buffer));
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, buffer, 6, 0) == 6 && std::memcmp(buffer, "aXYdef", 6) == 0);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, buffer, 4, -1) == SCE_FIOS_ERROR_BAD_OFFSET);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, buffer, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, nullptr, 4) == SCE_FIOS_ERROR_BAD_PTR);

    char first[2] = {};
    char second[3] = {};
    const SceFiosBuffer reads[2] = {{first, sizeof(first)}, {second, sizeof(second)}};
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, reads, 2, 1) == 5 && std::memcmp(first, "XY", 2) == 0 && std::memcmp(second, "def", 3) == 0);
    REQUIRE(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_SET) == 0);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, reads, 2) == 5 && std::memcmp(first, "aX", 2) == 0 && std::memcmp(second, "Yde", 3) == 0);
    const SceFiosBuffer writes[2] = {{const_cast<char*>("12"), 2}, {const_cast<char*>("345"), 3}};
    REQUIRE(sceFiosFHPwritevSync(nullptr, fh, writes, 2, 6) == 5 && sceFiosFHGetSize(fh) == 11);
    REQUIRE(sceFiosFHSeek(fh, 0, SCE_FIOS_SEEK_END) == 11);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, writes, 2) == 5 && sceFiosFHGetSize(fh) == 16);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, reads, 0) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, reads, 5000) == SCE_FIOS_ERROR_BAD_IOVCNT);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, nullptr, 1) == SCE_FIOS_ERROR_BAD_PTR);

    SceFiosStat stat{};
    REQUIRE(sceFiosFHStatSync(nullptr, fh, &stat) == SCE_FIOS_OK && stat.fileSize == 16);
    REQUIRE(sceFiosFHStatSync(nullptr, fh, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHTruncateSync(nullptr, fh, 4) == SCE_FIOS_OK && sceFiosFHGetSize(fh) == 4);
    REQUIRE(sceFiosFHTruncateSync(nullptr, fh, -1) == SCE_FIOS_ERROR_BAD_SIZE);
    REQUIRE(sceFiosFHSyncSync(nullptr, fh) == SCE_FIOS_OK);

    const SceFiosOp op = sceFiosFHPread(nullptr, fh, buffer, 4, 0);
    REQUIRE(sceFiosOpGetRequestCount(op) == 4 && sceFiosOpGetOffset(op) == 0 && path == sceFiosOpGetPath(op));
    REQUIRE(sceFiosOpSyncWaitForIO(op) == 4 && std::memcmp(buffer, "aXYd", 4) == 0);

    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
    REQUIRE(!sceFiosIsValidHandle(fh) && sceFiosFHGetPath(fh) == nullptr && sceFiosFHGetOpenParams(fh) == nullptr);
    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_ERROR_BAD_FH);
    REQUIRE(sceFiosFHReadSync(nullptr, fh, buffer, 1) == SCE_FIOS_ERROR_BAD_FH);
    REQUIRE(sceFiosFHSeek(fh, 0, 0) == SCE_FIOS_ERROR_BAD_FH && sceFiosFHTell(fh) == SCE_FIOS_ERROR_BAD_FH);
    REQUIRE(sceFiosFHGetSize(fh) == SCE_FIOS_ERROR_BAD_FH);
    REQUIRE(sceFiosFHStatSync(nullptr, fh, &stat) == SCE_FIOS_ERROR_BAD_FH);
    REQUIRE(sceFiosFHSyncSync(nullptr, fh) == SCE_FIOS_ERROR_BAD_FH);

    SceFiosOpenParams reader{};
    reader.openFlags = SCE_FIOS_O_READ;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &reader) == SCE_FIOS_OK);
    REQUIRE(sceFiosFHWriteSync(nullptr, fh, "z", 1) == SCE_FIOS_ERROR_READ_ONLY);
    REQUIRE(sceFiosFHTruncateSync(nullptr, fh, 1) == SCE_FIOS_ERROR_READ_ONLY);
    REQUIRE(sceFiosFHPreadSync(nullptr, fh, buffer, 4, 0) == 4);
    sceFiosCloseAllFiles();
    REQUIRE(!sceFiosIsValidHandle(fh) && sceFiosGetAllFHs(nullptr, 0) == 0);

    SceFiosOpenParams writeOnly{};
    writeOnly.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_TRUNC;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, path.c_str(), &writeOnly) == SCE_FIOS_OK);
    REQUIRE(sceFiosFHGetSize(fh) == 0 && sceFiosFHReadSync(nullptr, fh, buffer, 1) == SCE_FIOS_ERROR_WRITE_ONLY);
    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    SceFiosOpenParams appender{};
    appender.openFlags = SCE_FIOS_O_WRITE | SCE_FIOS_O_APPEND | SCE_FIOS_O_CREAT;
    const std::string created = directory + "/appended.bin";
    REQUIRE(sceFiosFHOpenWithModeSync(nullptr, &fh, created.c_str(), &appender, 0644) == SCE_FIOS_OK);
    REQUIRE(sceFiosFHWriteSync(nullptr, fh, "ab", 2) == 2 && sceFiosFHWriteSync(nullptr, fh, "cd", 2) == 2);
    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileGetSizeSync(nullptr, created.c_str()) == 4);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp opened = sceFiosFHOpen(&attr, &fh, created.c_str(), &reader);
    REQUIRE(g_completed == 1 && sceFiosOpGetError(opened) == SCE_FIOS_OK && sceFiosIsValidHandle(fh));
    sceFiosOpDelete(opened);
    const SceFiosOp closed = sceFiosFHClose(&attr, fh);
    REQUIRE(g_completed == 2 && sceFiosOpSyncWait(closed) == SCE_FIOS_OK);

    sceFiosSuspend();
    const SceFiosOp deferred = sceFiosFHOpen(nullptr, &fh, created.c_str(), &reader);
    REQUIRE(!sceFiosOpIsDone(deferred));
    sceFiosResume();
    REQUIRE(sceFiosOpSyncWait(deferred) == SCE_FIOS_OK && sceFiosIsValidHandle(fh));
    sceFiosShutdownAndCancelOps();
    REQUIRE(!sceFiosIsValidHandle(fh));
    REQUIRE(sceFiosDeleteSync(nullptr, created.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, path.c_str()) == SCE_FIOS_OK);
}

static void CheckDirectories(const std::string& directory) {
    const std::string root = directory + "/listing";
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, root.c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosFileWriteSync(nullptr, (root + "/one.bin").c_str(), "12345", 5, 0) == 5);
    REQUIRE(sceFiosFileWriteSync(nullptr, (root + "/two.bin").c_str(), "1", 1, 0) == 1);
    REQUIRE(sceFiosDirectoryCreateSync(nullptr, (root + "/inner").c_str()) == SCE_FIOS_OK);

    SceFiosDH dh = 0;
    char storage[256] = {};
    SceFiosBuffer small{storage, 1};
    SceFiosBuffer none{nullptr, 0};
    SceFiosBuffer big{storage, sizeof(storage)};
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), none) == SCE_FIOS_ERROR_BAD_SIZE);
    const SceFiosOp probe = sceFiosDHOpen(nullptr, &dh, root.c_str(), small);
    REQUIRE(sceFiosOpWait(probe) == SCE_FIOS_ERROR_BAD_SIZE);
    const std::int64_t required = sceFiosOpGetActualCount(probe);
    REQUIRE(required > 0 && required <= static_cast<std::int64_t>(sizeof(storage)));
    sceFiosOpDelete(probe);
    REQUIRE(dh == 0 && sceFiosGetAllDHs(nullptr, 0) == 0);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, (root + "/missing").c_str(), big) == SCE_FIOS_ERROR_BAD_PATH);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, (root + "/one.bin").c_str(), big) == SCE_FIOS_ERROR_NOT_A_DIRECTORY);
    REQUIRE(sceFiosDHOpenSync(nullptr, nullptr, root.c_str(), big) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, nullptr, big) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), big) == SCE_FIOS_OK);
    REQUIRE(sceFiosIsValidHandle(dh) && root == sceFiosDHGetPath(dh) && sceFiosGetAllDHs(nullptr, 0) == 1);

    int files = 0, directories = 0;
    SceFiosDirEntry entry{};
    for (;;) {
        std::memset(&entry, 0x7f, sizeof(entry));
        const int result = sceFiosDHReadSync(nullptr, dh, &entry);
        if (result == SCE_FIOS_ERROR_EOF) break;
        REQUIRE(result == SCE_FIOS_OK);
        const std::string name(entry.fullPath + entry.offsetToName, entry.nameLength);
        REQUIRE(std::string(entry.fullPath, entry.fullPathLength) == root + "/" + name && entry.fullPath[entry.fullPathLength] == 0);
        if (name == "inner") {
            ++directories;
            REQUIRE((entry.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0);
        } else {
            ++files;
            REQUIRE(entry.statFlags == 0 && entry.fileSize == (name == "one.bin" ? 5 : 1));
            REQUIRE(name == "one.bin" || name == "two.bin");
        }
    }
    REQUIRE(files == 2 && directories == 1);
    REQUIRE(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_EOF);
    REQUIRE(sceFiosDHReadSync(nullptr, dh, nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    REQUIRE(!sceFiosIsValidHandle(dh) && sceFiosDHGetPath(dh) == nullptr);
    REQUIRE(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_ERROR_BAD_DH);
    REQUIRE(sceFiosDHReadSync(nullptr, dh, &entry) == SCE_FIOS_ERROR_BAD_DH);

    g_completed = 0;
    const SceFiosOpAttr attr = CountingAttr();
    const SceFiosOp opened = sceFiosDHOpen(&attr, &dh, (root + "/").c_str(), big);
    REQUIRE(g_completed == 1 && sceFiosOpGetError(opened) == SCE_FIOS_OK);
    sceFiosOpDelete(opened);
    const SceFiosOp read = sceFiosDHRead(nullptr, dh, &entry);
    REQUIRE(sceFiosOpGetError(read) == SCE_FIOS_OK && entry.fullPathLength > root.size() + 1);
    REQUIRE(entry.fullPath[root.size()] == '/' && entry.fullPath[root.size() + 1] != '/');
    sceFiosOpDelete(read);
    REQUIRE(sceFiosDHCloseSync(nullptr, dh) == SCE_FIOS_OK);
    REQUIRE(sceFiosDHOpenSync(nullptr, &dh, root.c_str(), big) == SCE_FIOS_OK);
    sceFiosTerminate();
    REQUIRE(!sceFiosIsValidHandle(dh));
    REQUIRE(sceFiosDeleteSync(nullptr, (root + "/inner").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, (root + "/one.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, (root + "/two.bin").c_str()) == SCE_FIOS_OK);
    REQUIRE(sceFiosDeleteSync(nullptr, root.c_str()) == SCE_FIOS_OK);
}

static void CheckDates() {
    SceFiosTm tm{};
    tm.tm_year = 124;
    tm.tm_mon = 1;
    tm.tm_mday = 29;
    tm.tm_hour = 13;
    tm.tm_min = 14;
    tm.tm_sec = 15;
    const SceFiosDate date = sceFiosDateFromComponents(&tm);
    REQUIRE(date == 1709212455ull * 1000000000ull);
    SceFiosTm back{};
    REQUIRE(sceFiosDateToComponents(date + 999999999, &back) == &back);
    REQUIRE(back.tm_year == 124 && back.tm_mon == 1 && back.tm_mday == 29 && back.tm_hour == 13 && back.tm_min == 14 && back.tm_sec == 15);
    REQUIRE(back.tm_wday == 4 && back.tm_yday == 59 && back.tm_isdst == 0);
    REQUIRE(sceFiosDateToComponents(0, &back) == &back && back.tm_year == 70 && back.tm_mon == 0 && back.tm_mday == 1 && back.tm_wday == 4 && back.tm_yday == 0);
    REQUIRE(sceFiosDateToComponents(date, nullptr) == nullptr && sceFiosDateFromComponents(nullptr) == 0);
    SceFiosTm overflow{};
    overflow.tm_year = 99;
    overflow.tm_mon = 12;
    overflow.tm_mday = 32;
    overflow.tm_hour = 24;
    overflow.tm_min = 60;
    overflow.tm_sec = 60;
    SceFiosTm normalized{};
    sceFiosDateToComponents(sceFiosDateFromComponents(&overflow), &normalized);
    REQUIRE(normalized.tm_year == 100 && normalized.tm_mon == 1 && normalized.tm_mday == 2 && normalized.tm_hour == 1 && normalized.tm_min == 1 && normalized.tm_sec == 0);
    SceFiosTm negative{};
    negative.tm_year = 70;
    negative.tm_mon = -1;
    negative.tm_mday = 1;
    REQUIRE(sceFiosDateFromComponents(&negative) == 0);
    const SceFiosDate now = sceFiosDateGetCurrent();
    REQUIRE(now > 1700000000ull * 1000000000ull && sceFiosDateGetCurrent() >= now);
    REQUIRE(sceFiosTimeIntervalFromNanoseconds(-5) == -5 && sceFiosTimeIntervalToNanoseconds(123456789012) == 123456789012);
}

static void CheckBadPointers(const std::string& file) {
    const auto badAttr = reinterpret_cast<const SceFiosOpAttr*>(0x10);
    REQUIRE(sceFiosFileGetSizeSync(badAttr, file.c_str()) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFileGetSize(badAttr, file.c_str()) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(!sceFiosFileExistsSync(badAttr, file.c_str()));
    REQUIRE(sceFiosStatSync(badAttr, file.c_str(), nullptr) == SCE_FIOS_ERROR_BAD_PTR);
    sceFiosSetGlobalDefaultOpAttr(badAttr);
    SceFiosOpAttr stored{};
    REQUIRE(!sceFiosGetGlobalDefaultOpAttr(&stored));
    sceFiosSetThreadDefaultOpAttr(badAttr);
    REQUIRE(!sceFiosGetThreadDefaultOpAttr(&stored));

    const auto badParams = reinterpret_cast<const SceFiosOpenParams*>(0x10);
    SceFiosFH fh = 0;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), badParams) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHOpenWithModeSync(nullptr, &fh, file.c_str(), badParams, -1) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(fh == 0 && sceFiosGetAllFHs(nullptr, 0) == 0);

    SceFiosOpenParams reader{};
    reader.openFlags = SCE_FIOS_O_READ;
    REQUIRE(sceFiosFHOpenSync(nullptr, &fh, file.c_str(), &reader) == SCE_FIOS_OK);
    const auto badVectors = reinterpret_cast<const SceFiosBuffer*>(0x10);
    REQUIRE(sceFiosFHReadvSync(nullptr, fh, badVectors, 2) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHPreadvSync(nullptr, fh, badVectors, 2, 0) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHWritevSync(nullptr, fh, badVectors, 2) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(sceFiosFHCloseSync(nullptr, fh) == SCE_FIOS_OK);

    REQUIRE(sceFiosDateFromComponents(reinterpret_cast<const SceFiosTm*>(0x10)) == 0);
    REQUIRE(sceFiosInitialize(reinterpret_cast<const SceFiosParams*>(0x10)) == SCE_FIOS_ERROR_BAD_PTR);
    REQUIRE(!sceFiosIsInitialized(nullptr));
    sceFiosUpdateParameters(reinterpret_cast<const SceFiosParams*>(0x10));
}

int main() {
    const std::filesystem::path directory = "fios2_probe";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::string file = (directory / "data.bin").generic_string();
    {
        std::ofstream out(file, std::ios::binary);
        out << "0123456789";
    }
    CheckInitialization();
    CheckQueries(directory.generic_string(), file);
    CheckOperationLifecycle(file);
    CheckCallbacks(file);
    CheckSuspension(file);
    CheckMutations(directory.generic_string());
    CheckFileHandles(directory.generic_string());
    CheckDirectories(directory.generic_string());
    CheckDates();
    CheckBadPointers(file);
    sceFiosTerminate();
    REQUIRE(!sceFiosIsInitialized(nullptr));
    std::filesystem::remove_all(directory);
    std::puts("guest_fios2 ok");
    return 0;
}

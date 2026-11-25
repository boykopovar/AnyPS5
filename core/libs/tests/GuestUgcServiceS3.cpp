#include "prx/libSceUgcServiceS3/libSceUgcServiceS3.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <thread>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrNoClient = static_cast<int>(0x81b70002u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x81b70003u);
constexpr int kErrInvalidArg = static_cast<int>(0x81b70004u);
constexpr int kErrUnsupportedFileType = static_cast<int>(0x81b71001u);
constexpr int kErrNotFoundRequest = static_cast<int>(0x81b71005u);
constexpr int kErrUploadedInfoEmpty = static_cast<int>(0x81b71006u);

struct PartInfo {
    std::uint64_t reserved = 0;
    std::uint32_t partIndex = 0;
    std::uint32_t pad = 0;
};

void TestUninitializedBehavior() {
    float progress = 0.5f;
    std::uint64_t progressSize = 123;
    std::uint64_t outSize = 0;
    std::array<char, 64> info{};
    PartInfo part{};

    for (int pass = 0; pass < 2; ++pass) {
        Require(sceUgcServiceS3Unknown01() == 0);
        Require(sceUgcServiceS3CreateRequest(nullptr) == kErrNoClient);
        Require(sceUgcServiceS3Unknown02(1) == kErrNoClient);
        Require(sceUgcServiceS3CancelRequest(1) == kErrNoClient);
        Require(sceUgcServiceS3Unknown03(1, nullptr) == kErrNoClient);
        Require(sceUgcServiceS3Unknown04(1, nullptr) == kErrNoClient);
        Require(sceUgcServiceS3Unknown05(1, &progress) == kErrNoClient);
        Require(sceUgcServiceS3Unknown06(1, &progressSize) == kErrNoClient);
        Require(sceUgcServiceS3UploadFileWithGmsResponse(1, 1, "/tmp/a", "{}") == kErrNoClient);
        Require(sceUgcServiceS3Unknown07(1, 1, "/tmp/a", "{}", &part, nullptr) == kErrNoClient);
        Require(sceUgcServiceS3Unknown08(1, 1, "/tmp/a", "{}", &part, nullptr) == kErrNoClient);
        Require(sceUgcServiceS3DownloadFileWithGmsResponse(1, 1, "/tmp/a", &outSize, "{}") == kErrNoClient);
        Require(sceUgcServiceS3Unknown09(1, info.data(), info.size()) == kErrNoClient);
    }
}

void TestClientLifecycleAndRequests() {
    Require(sceUgcServiceS3Unknown00() == 0);
    Require(sceUgcServiceS3Unknown00() == kErrAlreadyInitialized);

    Require(sceUgcServiceS3CreateRequest(reinterpret_cast<const char*>(0x10)) == kErrInvalidArg);

    const int req1 = sceUgcServiceS3CreateRequest(nullptr);
    const int req2 = sceUgcServiceS3CreateRequest("access");
    Require(req1 == 1);
    Require(req2 == 2);

    int dummy = 0;
    Require(sceUgcServiceS3Unknown03(req1, &dummy) == 0);
    Require(sceUgcServiceS3Unknown04(req1, &dummy) == 0);
    Require(sceUgcServiceS3Unknown03(9999, &dummy) == kErrNotFoundRequest);
    Require(sceUgcServiceS3Unknown04(9999, &dummy) == kErrNotFoundRequest);

    float progress = 0.5f;
    Require(sceUgcServiceS3Unknown05(req1, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown05(9999, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown05(9999, reinterpret_cast<float*>(0x10)) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown05(9999, &progress) == kErrNotFoundRequest);
    Require(progress == 0.5f);
    Require(sceUgcServiceS3Unknown05(req1, &progress) == 0);
    Require(progress == 0.0f);

    std::uint64_t progressSize = 0x5555555555555555ULL;
    Require(sceUgcServiceS3Unknown06(req1, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown06(9999, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown06(9999, reinterpret_cast<std::uint64_t*>(0x10)) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown06(9999, &progressSize) == kErrNotFoundRequest);
    Require(progressSize == 0x5555555555555555ULL);
    Require(sceUgcServiceS3Unknown06(req1, &progressSize) == 0);
    Require(progressSize == 0);

    std::array<char, 64> info{};
    Require(sceUgcServiceS3Unknown09(req1, nullptr, 16) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown09(req1, info.data(), 0) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown09(9999, info.data(), info.size()) == kErrNotFoundRequest);
    Require(sceUgcServiceS3Unknown09(req1, info.data(), info.size()) == kErrUploadedInfoEmpty);

    std::uint64_t outSize = 0;
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 1, nullptr, "{}") == kErrInvalidArg);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 1, reinterpret_cast<const char*>(0x10), "{}") == kErrInvalidArg);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(9999, 1, "/tmp/a", "{}") == kErrNotFoundRequest);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 5, "/tmp/a", "{}") == kErrUnsupportedFileType);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 0, "/tmp/a", "{}") == kErrUnsupportedFileType);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 1, "/tmp/a", nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req1, 1, "/tmp/a", reinterpret_cast<const char*>(0x10)) == kErrInvalidArg);

    PartInfo part{};
    PartInfo badPart{};
    badPart.partIndex = 0x2711u;
    Require(sceUgcServiceS3Unknown07(req1, 1, nullptr, "{}", &part, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown07(req1, 1, "/tmp/a", "{}", nullptr, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown07(req1, 1, "/tmp/a", "{}", reinterpret_cast<const void*>(0x10), nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown07(req1, 1, "/tmp/a", "{}", &badPart, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown07(9999, 1, "/tmp/a", "{}", &part, nullptr) == kErrNotFoundRequest);
    Require(sceUgcServiceS3Unknown07(req1, 5, "/tmp/a", "{}", &part, nullptr) == kErrUnsupportedFileType);
    Require(sceUgcServiceS3Unknown07(req1, 1, "/tmp/a", nullptr, &part, nullptr) == kErrInvalidArg);

    Require(sceUgcServiceS3Unknown08(req1, 1, nullptr, "{}", &part, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown08(req1, 1, "/tmp/a", "{}", &badPart, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown08(9999, 1, "/tmp/a", "{}", &part, nullptr) == kErrNotFoundRequest);
    Require(sceUgcServiceS3Unknown08(req1, 5, "/tmp/a", "{}", &part, nullptr) == kErrUnsupportedFileType);
    Require(sceUgcServiceS3Unknown08(req1, 1, "/tmp/a", nullptr, &part, nullptr) == kErrInvalidArg);

    Require(sceUgcServiceS3DownloadFileWithGmsResponse(req1, 1, nullptr, &outSize, "{}") == kErrInvalidArg);
    Require(sceUgcServiceS3DownloadFileWithGmsResponse(req1, 1, reinterpret_cast<const char*>(0x10), &outSize, "{}") == kErrInvalidArg);
    Require(sceUgcServiceS3DownloadFileWithGmsResponse(9999, 1, "/tmp/a", &outSize, "{}") == kErrNotFoundRequest);
    Require(sceUgcServiceS3DownloadFileWithGmsResponse(req1, 0, "/tmp/a", &outSize, "{}") == kErrUnsupportedFileType);
    Require(sceUgcServiceS3DownloadFileWithGmsResponse(req1, 6, "/tmp/a", &outSize, "{}") == kErrUnsupportedFileType);
    Require(sceUgcServiceS3DownloadFileWithGmsResponse(req1, 5, "/tmp/a", &outSize, nullptr) == kErrInvalidArg);

    Require(sceUgcServiceS3CancelRequest(9999) == kErrNotFoundRequest);
    Require(sceUgcServiceS3CancelRequest(req1) == 0);
    Require(sceUgcServiceS3Unknown02(req1) == 0);
    Require(sceUgcServiceS3Unknown02(req1) == kErrNotFoundRequest);
    Require(sceUgcServiceS3CancelRequest(req1) == kErrNotFoundRequest);

    Require(sceUgcServiceS3Unknown01() == 0);
    Require(sceUgcServiceS3Unknown01() == 0);
    Require(sceUgcServiceS3CreateRequest(nullptr) == kErrNoClient);
    Require(sceUgcServiceS3CancelRequest(req2) == kErrNoClient);

    Require(sceUgcServiceS3Unknown00() == 0);
    Require(sceUgcServiceS3CreateRequest(nullptr) == 1);
    Require(sceUgcServiceS3Unknown01() == 0);
}

void TestWraparoundAndConcurrentDeinit() {
    Require(sceUgcServiceS3Unknown00() == 0);
    const int req = sceUgcServiceS3CreateRequest(nullptr);
    Require(req > 0);

    constexpr std::uintptr_t kMaxAddr = (std::numeric_limits<std::uintptr_t>::max)();
    const auto* nearEndStr = reinterpret_cast<const char*>(kMaxAddr);
    const auto* nearEndMid = reinterpret_cast<const char*>(kMaxAddr - 3u);
    const auto* nearEndPart = reinterpret_cast<const void*>(kMaxAddr - 4u);
    Require(sceUgcServiceS3CreateRequest(nearEndStr) == kErrInvalidArg);
    Require(sceUgcServiceS3CreateRequest(nearEndMid) == kErrInvalidArg);
    Require(sceUgcServiceS3UploadFileWithGmsResponse(req, 1, nearEndMid, "{}") == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown07(req, 1, "/tmp/a", "{}", nearEndPart, nullptr) == kErrInvalidArg);
    Require(sceUgcServiceS3Unknown02(req) == 0);
    Require(sceUgcServiceS3Unknown01() == 0);

    std::atomic<bool> done{false};
    std::thread lifecycle([&]() {
        for (int i = 0; i < 200; ++i) {
            (void)sceUgcServiceS3Unknown00();
            (void)sceUgcServiceS3Unknown01();
        }
        done.store(true, std::memory_order_release);
    });
    std::thread requests([&]() {
        while (!done.load(std::memory_order_acquire)) {
            const int id = sceUgcServiceS3CreateRequest(nullptr);
            Require(id > 0 || id == kErrNoClient);
            if (id > 0) {
                const int cancelRet = sceUgcServiceS3CancelRequest(id);
                Require(cancelRet == 0 || cancelRet == kErrNoClient || cancelRet == kErrNotFoundRequest);
                const int delRet = sceUgcServiceS3Unknown02(id);
                Require(delRet == 0 || delRet == kErrNoClient || delRet == kErrNotFoundRequest);
            }
        }
    });
    lifecycle.join();
    requests.join();
    Require(sceUgcServiceS3Unknown01() == 0);
}

}

int main() {
    TestUninitializedBehavior();
    TestClientLifecycleAndRequests();
    TestWraparoundAndConcurrentDeinit();
    return 0;
}

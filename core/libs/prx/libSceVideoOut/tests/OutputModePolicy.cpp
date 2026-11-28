#include "prx/libSceVideoOut/include/FrameTimingLog.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

namespace {

struct ScopedTempDir {
    std::filesystem::path original;
    std::filesystem::path path;

    ScopedTempDir() : original(std::filesystem::current_path()) {
        const auto base = std::filesystem::temp_directory_path();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 1000; ++attempt) {
            const auto candidate = base / ("anyps5-videoout-test-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                path = candidate;
                std::filesystem::current_path(path);
                return;
            }
        }
        Require(false, "creating temporary test directory failed");
    }

    ~ScopedTempDir() {
        std::error_code error;
        std::filesystem::current_path(original, error);
        if (!path.empty()) {
            std::filesystem::remove_all(path, error);
        }
    }
};

struct CustomLogFailure : std::runtime_error {
    int code;
    CustomLogFailure(const char* message, int value) : std::runtime_error(message), code(value) {}
};

void TestFrameTimingLog() {
    ScopedTempDir tempDir;

    {
        FrameTimingLog log;
        log.Enqueue([](std::ostream& out) { out << "before\n"; });
        bool rejected = false;
        try {
            log.Enqueue(FrameTimingLog::Report{});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "empty FrameTimingLog report was not rejected with invalid_argument");
        log.Enqueue([](std::ostream& out) { out << "after\n"; });
        log.Finish();

        std::ifstream input("frame-timing.log", std::ios::binary);
        Require(static_cast<bool>(input), "opening frame-timing.log for verification failed");
        const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Require(contents == "before\nafter\n", "frame-timing.log did not contain both before and after reports");
    }
    Require(std::filesystem::remove("frame-timing.log"), "removing frame-timing.log after empty report test failed");

    {
        FrameTimingLog log;
        log.Enqueue([](std::ostream& out) { out << "partial\n"; });
        log.Enqueue([](std::ostream&) { throw CustomLogFailure("boom", 42); });
        log.Enqueue([](std::ostream& out) { out << "never\n"; });

        int caughtCode = 0;
        try {
            log.Finish();
        } catch (const CustomLogFailure& err) {
            caughtCode = err.code;
        }
        Require(caughtCode == 42, "Finish did not rethrow the exact custom exception");

        std::error_code removeError;
        const bool removed = std::filesystem::remove("frame-timing.log", removeError);
        Require(removed && !removeError, "frame-timing.log remained open after Finish failed");

        int repeatedCode = 0;
        try {
            log.Finish();
        } catch (const CustomLogFailure& err) {
            repeatedCode = err.code;
        }
        Require(repeatedCode == 42, "repeated Finish did not rethrow the stored failure");
    }

    {
        FrameTimingLog log;
        log.Enqueue([](std::ostream&) { throw CustomLogFailure("concurrent", 77); });
        std::atomic<int> rethrownCount{0};
        std::vector<std::thread> callers;
        for (int i = 0; i < 4; ++i) {
            callers.emplace_back([&] {
                try {
                    log.Finish();
                } catch (const CustomLogFailure& err) {
                    if (err.code == 77) rethrownCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& caller : callers) caller.join();
        Require(rethrownCount.load() == 4, "concurrent Finish calls did not all observe the stored failure");

        std::error_code removeError;
        Require(std::filesystem::remove("frame-timing.log", removeError) && !removeError, "frame-timing.log remained open after concurrent Finish failure");
    }

    {
        const auto start = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < 20; ++iteration) {
            std::mutex stepMutex;
            std::condition_variable stepCv;
            bool ran = false;
            {
                FrameTimingLog log;
                log.Enqueue([&](std::ostream& out) {
                    out << iteration << '\n';
                    {
                        std::lock_guard lock(stepMutex);
                        ran = true;
                    }
                    stepCv.notify_one();
                });
                {
                    std::unique_lock lock(stepMutex);
                    stepCv.wait(lock, [&] { return ran; });
                }
                if ((iteration % 2) == 0) {
                    log.Finish();
                }
            }
        }
        for (int iteration = 0; iteration < 10; ++iteration) {
            FrameTimingLog log;
            log.Enqueue([&](std::ostream& out) { out << iteration << '\n'; });
            if ((iteration % 2) == 0) {
                log.Finish();
            }
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        Require(elapsed < std::chrono::seconds(5), "FrameTimingLog shutdown stalled during rapid Enqueue/Finish cycles");

        std::error_code removeError;
        Require(std::filesystem::remove("frame-timing.log", removeError) && !removeError, "removing frame-timing.log after rapid cycle test failed");
    }
}

}

int main() {
    constexpr int unsupported = static_cast<int>(0x80290016);
    constexpr int invalidValue = static_cast<int>(0x80290001);
    constexpr int unknownMode = static_cast<int>(0x8029001e);
    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0, "output options initialization failed");
    for (int refresh : {-1, 0, 30, 59, 60, 119, 120, 144, 240}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(15, &options, refresh) == (refresh < 120 ? unsupported : 0), "mode 15 did not follow the display refresh threshold");
        Require(ValidateVideoOutOutputMode_nid_no_patch(15, nullptr, refresh) == (refresh < 120 ? unsupported : 0), "mode 15 without options did not follow the display refresh threshold");
        Require(ValidateVideoOutOutputMode_nid_no_patch(1, &options, refresh) == 0, "default mode depended on the display refresh rate");
    }
    for (std::uint64_t mode : {4, 7, 8, 12, 13, 14, 16, 17, 19}) {
        Require(ValidateVideoOutOutputMode_nid_no_patch(mode, &options, 240) == unsupported, "unimplemented output mode was accepted on a fast display");
    }
    Require(ValidateVideoOutOutputMode_nid_no_patch(0xd000000a, &options, 240) == unknownMode, "unknown output mode was accepted");
    for (int refresh : {59, 60, 119, 120, 144}) {
        Require(VideoOutOutputSupport_nid_no_patch(15, &options, refresh) == (refresh < 120 ? 0 : 1), "mode 15 support did not answer 0 or 1 for the display");
        Require(VideoOutOutputSupport_nid_no_patch(1, &options, refresh) == 1, "default mode was not supported");
        Require(VideoOutOutputSupport_nid_no_patch(4, &options, refresh) == unsupported, "mode 4 did not stay unsupported");
        Require(VideoOutOutputSupport_nid_no_patch(0xd000000a, &options, refresh) == unknownMode, "unknown mode did not stay unknown");
    }
    VideoOutOutputOptions changed = options;
    changed.internalData[0] = 0;
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 120) == invalidValue, "mode 15 accepted invalid output options");
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 60) == unsupported, "invalid options changed unsupported-mode error precedence");
    changed = options;
    changed.internalData[3] = 0xffffffff;
    Require(ValidateVideoOutOutputMode_nid_no_patch(15, &changed, 120) == 0, "mode 15 rejected the free options word");
    Require(ValidateVideoOutOutputMode_nid_no_patch(18, &options, 120) == unknownMode, "mode 18 was not answered with the unknown-mode code");
    TestFrameTimingLog();
}

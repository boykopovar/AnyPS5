#include "prx/libSceImageUtil/libSceImageUtil.hpp"

#include "Decoder/Jpeg.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct GuestString {
    union {
        char bytes[16];
        const char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(GuestString) == 32);

constexpr auto ReadDimensions = libSceImageUtilUnknown_pjWv_plus__plus_MzaJY_nid_no_patch;
constexpr auto RegisterCallback = libSceImageUtilUnknown_J805kfu8t4w_nid_no_patch;
constexpr auto UnregisterCallback = libSceImageUtilUnknown_TBLqZYmwmnU_nid_no_patch;
constexpr auto StopAgentServer = libSceImageUtilUnknown_PaJr2GfStvA_nid_no_patch;

struct CallbackState {
    std::uint32_t calls;
    std::uint32_t line;
    std::uint64_t kind;
    std::int32_t error_code;
};

struct ReentryState {
    std::uint32_t calls;
    int unregister_rc;
    int register_rc;
    CallbackState* followup;
};

void OnImageUtilError(const void* report, void* user_data) {
    auto* state = static_cast<CallbackState*>(user_data);
    ++state->calls;
    if (report) {
        std::memcpy(&state->line, report, sizeof(state->line));
        std::memcpy(&state->kind, static_cast<const std::uint8_t*>(report) + 8, sizeof(state->kind));
        std::memcpy(&state->error_code, static_cast<const std::uint8_t*>(report) + 16, sizeof(state->error_code));
    }
}

void OnReentryB(const void*, void* user_data);

void OnReentryA(const void*, void* user_data) {
    auto* state = static_cast<ReentryState*>(user_data);
    if (state->calls++ == 0) {
        state->unregister_rc = UnregisterCallback(&OnReentryB);
        state->register_rc = RegisterCallback(&OnImageUtilError, state->followup);
    }
}

void OnReentryB(const void*, void* user_data) {
    auto* state = static_cast<ReentryState*>(user_data);
    if (state->calls++ == 0) {
        state->unregister_rc = UnregisterCallback(&OnReentryA);
        state->register_rc = RegisterCallback(&OnImageUtilError, state->followup);
    }
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ImageUtil: %s\n", message);
    std::abort();
}

void WriteFile(const char* filename, std::span<const std::uint8_t> bytes) {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close();
    Require(file.good(), "write JPEG fixture");
}

void TestDimensions(const char* filename, std::uint32_t width, std::uint32_t height) {
    const std::string text = std::string("/") + filename;
    GuestString path{};
    path.size = text.size();
    if (text.size() < sizeof(path.bytes)) {
        path.capacity = sizeof(path.bytes) - 1;
        std::memcpy(path.bytes, text.c_str(), text.size() + 1);
    } else {
        path.capacity = text.capacity();
        path.pointer = text.c_str();
    }

    const auto image = Decoder::Jpeg::Encode(std::vector<std::uint8_t>(width * height * 3, 91), width, height, 3, 85);
    Require(!image.empty(), "encode JPEG fixture");
    WriteFile(filename, image);

    constexpr std::uint64_t marker = 0xABABABABABABABAB;
    std::array output{marker, marker, marker, marker};
    Require(ReadDimensions(&path, output.data() + 1) == 0, "read JPEG dimensions");
    Require(output == std::array{marker, std::uint64_t{width}, std::uint64_t{height}, marker},
            "write exactly the two zero-extended dimensions");

    output.fill(marker);
    const auto unchanged = output;
    WriteFile(filename, std::span(image).first(8));
    Require(ReadDimensions(&path, output.data() + 1) == static_cast<int>(0x809D4050u), "reject truncated JPEG");
    Require(output == unchanged, "truncated JPEG changed output");

    std::filesystem::remove(filename);
    Require(ReadDimensions(&path, output.data() + 1) == static_cast<int>(0x809D401Eu), "report missing file");
    Require(output == unchanged, "missing file changed output");
    Require(ReadDimensions(&path, nullptr) == static_cast<int>(0x809D4013u), "check null output before stat");
}

void TestCallbacksAndAgentStop() {
    Require(StopAgentServer() == 0, "stop agent server when idle");

    CallbackState state{};
    std::memset(&state, 0xA5, sizeof(state));
    state.calls = 0;

    Require(RegisterCallback(&OnImageUtilError, &state) == 0, "register error callback");
    Require(RegisterCallback(&OnImageUtilError, &state) == static_cast<int>(0x809D4013u),
            "reject duplicate error callback");

    const std::string missing = "/imutil_missing_cb_test.jpg";
    GuestString path{};
    path.size = missing.size();
    path.capacity = missing.capacity();
    path.pointer = missing.c_str();
    std::array<std::uint64_t, 2> dims{0, 0};
    Require(ReadDimensions(&path, dims.data()) == static_cast<int>(0x809D401Eu), "missing file returns error");
    Require(state.calls == 1 && state.line == 0x2e7u && state.kind == 0 && state.error_code == 0,
            "missing file invokes registered callback");

    Require(UnregisterCallback(&OnImageUtilError) == 0, "unregister error callback");
    Require(ReadDimensions(&path, dims.data()) == static_cast<int>(0x809D401Eu), "missing file after unregister");
    Require(state.calls == 1, "unregistered callback is not invoked");
    Require(RegisterCallback(&OnImageUtilError, &state) == 0, "re-register after unregister");
    Require(UnregisterCallback(&OnImageUtilError) == 0, "cleanup callback");

    CallbackState followup{};
    ReentryState reentry{0, -1, -1, &followup};
    Require(RegisterCallback(&OnReentryA, &reentry) == 0, "register first reentry callback");
    Require(RegisterCallback(&OnReentryB, &reentry) == 0, "register second reentry callback");

    std::mutex done_mutex;
    std::condition_variable done_cv;
    bool done = false;
    int reentry_read_rc = 0;
    std::thread worker([&] {
        reentry_read_rc = ReadDimensions(&path, dims.data());
        {
            std::lock_guard lock(done_mutex);
            done = true;
        }
        done_cv.notify_one();
    });
    {
        std::unique_lock lock(done_mutex);
        if (!done_cv.wait_for(lock, std::chrono::seconds(2), [&] { return done; })) {
            worker.detach();
            Require(false, "callback re-entry deadlocked");
        }
    }
    worker.join();
    Require(reentry_read_rc == static_cast<int>(0x809D401Eu), "missing file returns error on re-entry");
    Require(reentry.calls == 2 && reentry.unregister_rc == 0 && reentry.register_rc == 0,
            "snapshot invokes both callbacks even after re-entrant unregister and register");
    Require(followup.calls == 0, "newly registered callback is not invoked in same notification pass");

    Require(ReadDimensions(&path, dims.data()) == static_cast<int>(0x809D401Eu), "missing file invokes followup");
    Require(reentry.calls == 3 && followup.calls == 1,
            "remaining reentry callback and newly registered followup run on next notification");
    Require(UnregisterCallback(&OnReentryA) == 0, "cleanup first reentry callback");
    Require(UnregisterCallback(&OnReentryB) == 0, "cleanup second reentry callback");
    Require(UnregisterCallback(&OnImageUtilError) == 0, "cleanup followup callback");
}

}

int main() {
    std::array<std::uint64_t, 2> output{19, 27};
    Require(ReadDimensions(nullptr, output.data()) == static_cast<int>(0x809D4013u), "reject null path");
    Require(output == std::array<std::uint64_t, 2>{19, 27}, "null path changed output");

    GuestString invalid{};
    invalid.capacity = 15;
    invalid.size = 16;
    bool rejected = false;
    try {
        ReadDimensions(&invalid, output.data());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && output == std::array<std::uint64_t, 2>{19, 27}, "reject inconsistent guest string");

    TestDimensions("imutil.jpg", 19, 7);
    TestDimensions("image_util_dimensions_test.jpg", 41, 13);
    TestCallbacksAndAgentStop();
}
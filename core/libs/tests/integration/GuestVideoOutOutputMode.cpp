#include "SceTypes.hpp"
#include "GuestVideoOutFixture.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options);
int APS5_VABI sceVideoOutIsOutputSupported(int handle, std::uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, std::uint64_t reserved);
int APS5_VABI sceVideoOutConfigureOutput(int handle, std::uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, std::uint64_t reserved);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

VideoOutOutputOptions DefaultOptions() {
    VideoOutOutputOptions options{};
    RequireEqual(sceVideoOutInitializeOutputOptions(&options), 0, "initialize the output options");
    return options;
}

std::string Mode(std::uint64_t mode) {
    return "mode " + std::to_string(mode);
}

const Case initialize{"InitializeOutputOptions_Garbage_WritesDefaultLayout", [] {
    std::array<std::uint8_t, sizeof(VideoOutOutputOptions)> raw{};
    raw.fill(0xAA);
    VideoOutOutputOptions options{};
    std::memcpy(&options, raw.data(), raw.size());
    RequireEqual(sceVideoOutInitializeOutputOptions(&options), 0, "initialize");
    std::memcpy(raw.data(), &options, raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        RequireEqual(raw[index], index == 2 ? 0xFF : 0, "byte " + std::to_string(index));
    }
}};

const Case initializeNull{"InitializeOutputOptions_Null_FailsInvalidAddress", [] {
    RequireEqual(sceVideoOutInitializeOutputOptions(nullptr), INVALID_ADDRESS, "null options");
}};

const Case unopened{"IsOutputSupported_UnopenedHandle_FailsInvalidHandle", [] {
    const AppDirectory app;
    const auto options = DefaultOptions();
    SkipWithoutDisplay([&] {
        for (int handle : {-1, 0, 1, 0x7FFFFFFF}) {
            RequireEqual(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0), INVALID_HANDLE, "handle " + std::to_string(handle));
            RequireEqual(sceVideoOutIsOutputSupported(handle, 2, nullptr, const_cast<VideoOutOutputOptions*>(&options), 1), INVALID_HANDLE,
                         "handle " + std::to_string(handle) + " with reserved arguments");
        }
    });
}};

const Case defaultMode{"IsOutputSupported_DefaultMode_IsSupportedWithOrWithoutOptions", [] {
    const OpenVideoOut port;
    const auto options = DefaultOptions();
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &options, nullptr, 0), 1, "with options");
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, nullptr, nullptr, 0), 1, "without options");
}};

const Case unsupportedModes{"OutputMode_KnownUnavailableModes_FailUnsupported", [] {
    const OpenVideoOut port;
    const auto options = DefaultOptions();
    for (std::uint64_t mode : {0x4ull, 0x7ull, 0x8ull, 0xCull, 0xDull, 0xEull, 0xFull, 0x10ull, 0x11ull, 0x13ull}) {
        RequireEqual(sceVideoOutIsOutputSupported(port.handle, mode, &options, nullptr, 0), UNSUPPORTED_OUTPUT_MODE, "query " + Mode(mode));
        RequireEqual(sceVideoOutConfigureOutput(port.handle, mode, &options, nullptr, 0), UNSUPPORTED_OUTPUT_MODE, "configure " + Mode(mode));
    }
}};

const Case unknownModes{"IsOutputSupported_UnknownModes_FailUnknown", [] {
    const OpenVideoOut port;
    const auto options = DefaultOptions();
    for (std::uint64_t mode : {0x0ull, 0x2ull, 0x3ull, 0x5ull, 0x6ull, 0x9ull, 0xAull, 0xBull, 0x14ull, 0x20ull, 0x3Full, 0x100ull,
                               0xD000000Aull, 0x8000000000000001ull, ~0ull}) {
        RequireEqual(sceVideoOutIsOutputSupported(port.handle, mode, &options, nullptr, 0), UNKNOWN_OUTPUT_MODE, Mode(mode));
    }
}};

const Case unimplementedMode{"IsOutputSupported_Mode0x12_Throws", [] {
    const OpenVideoOut port;
    const auto options = DefaultOptions();
    Testing::RequireThrows<std::runtime_error>([&] { sceVideoOutIsOutputSupported(port.handle, 0x12, &options, nullptr, 0); }, "mode 0x12");
}};

const Case invalidArguments{"IsOutputSupported_ReservedArgumentsOrZeroedOptions_FailInvalidValue", [] {
    const OpenVideoOut port;
    auto options = DefaultOptions();
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &options, &options, 0), INVALID_VALUE, "reserved pointer");
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &options, nullptr, 1), INVALID_VALUE, "reserved value");
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 2, &options, nullptr, 1), INVALID_VALUE, "reserved value with mode 2");
    const VideoOutOutputOptions zeroed{};
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &zeroed, nullptr, 0), INVALID_VALUE, "zeroed options with mode 1");
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 2, &zeroed, nullptr, 0), UNKNOWN_OUTPUT_MODE, "zeroed options with mode 2");
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 4, &zeroed, nullptr, 0), UNSUPPORTED_OUTPUT_MODE, "zeroed options with mode 4");
}};

const Case changedWords{"IsOutputSupported_ChangedOptionWord_OnlyWord3IsFree", [] {
    const OpenVideoOut port;
    const auto options = DefaultOptions();
    for (std::size_t word = 0; word < 16; ++word) {
        VideoOutOutputOptions changed = options;
        changed.internalData[word] ^= 1;
        RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &changed, nullptr, 0), word == 3 ? 1 : INVALID_VALUE,
                     "word " + std::to_string(word) + " flipped");
    }
    VideoOutOutputOptions freeWord = options;
    freeWord.internalData[3] = 0xFFFFFFFF;
    RequireEqual(sceVideoOutIsOutputSupported(port.handle, 1, &freeWord, nullptr, 0), 1, "word 3 all ones");
}};

const Case configureDefault{"ConfigureOutput_DefaultModeThenClose_SucceedsAndInvalidatesHandle", [] {
    OpenVideoOut port;
    const auto options = DefaultOptions();
    RequireEqual(sceVideoOutConfigureOutput(port.handle, 1, &options, nullptr, 0), 0, "configure the default mode");
    const int handle = port.handle;
    port.Close();
    RequireEqual(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0), INVALID_HANDLE, "query after close");
}};

} // namespace

int main(int argc, char** argv) {
    return RunVideoOutTests(argc, argv);
}

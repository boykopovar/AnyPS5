#include "prx/libSceSrcUtl/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

extern "C" [[gnu::weak, noreturn]] void NotImplemented_nid_no_patch(const char*) {
    std::abort();
}

namespace {

constexpr int INVALID_HANDLE = static_cast<int>(0x815D0000);
constexpr int INVALID_POINTER = static_cast<int>(0x815D0001);
constexpr int INVALID_CHANNELS = static_cast<int>(0x815D0002);
constexpr int INVALID_MODE = static_cast<int>(0x815D0003);
constexpr int INVALID_INPUT_RATE = static_cast<int>(0x815D0004);
constexpr int INVALID_OUTPUT_RATE = static_cast<int>(0x815D0005);
constexpr int HANDLE_SIZE = 0x9678;
constexpr std::uint64_t MAGIC = 0xDEADBEEFBAADF00Dull;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void CheckHandleSize() {
    constexpr std::uint32_t inputs[] = {22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000};
    constexpr std::uint32_t outputs[] = {44100, 48000, 96000, 192000};
    constexpr std::uint32_t rates[] = {0, 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000,
        128000, 176400, 192000, 384000, 0xFFFFFFFF};
    for (std::uint32_t channels = 0; channels <= 9; ++channels) {
        for (std::uint32_t mode = 0; mode <= 3; ++mode) {
            for (const std::uint32_t input : rates) {
                for (const std::uint32_t output : rates) {
                    int expected = HANDLE_SIZE;
                    if (channels == 0 || channels > 8) expected = INVALID_CHANNELS;
                    else if (mode > 2) expected = INVALID_MODE;
                    else if (std::find(std::begin(inputs), std::end(inputs), input) == std::end(inputs)) expected = INVALID_INPUT_RATE;
                    else if (std::find(std::begin(outputs), std::end(outputs), output) == std::end(outputs)) expected = INVALID_OUTPUT_RATE;
                    Require(sceSrcUtilityGetHandleSize(channels, mode, input, output) == expected, "handle size checks");
                }
            }
        }
    }
    Require(sceSrcUtilityGetHandleSize(0xFFFFFFFF, 0xFFFFFFFF, 0, 0) == INVALID_CHANNELS, "channels are checked first");
    Require(sceSrcUtilityGetHandleSize(2, 0xFFFFFFFF, 0, 0) == INVALID_MODE, "the mode is checked second");
    Require(sceSrcUtilityGetHandleSize(2, 0, 22050, 192000) == HANDLE_SIZE, "22050 to 192000");
}

void CheckPriority() {
    Require(sceSrcUtilitySetPriority(nullptr, 1) == INVALID_HANDLE, "set on a null handle");
    std::uint32_t priority = 7;
    Require(sceSrcUtilityGetPriority(nullptr, &priority) == INVALID_HANDLE && priority == 7, "get on a null handle");
    std::vector<std::uint8_t> handle(HANDLE_SIZE, 0xAB);
    const std::vector<std::uint8_t> untouched = handle;
    Require(sceSrcUtilitySetPriority(handle.data(), 1) == INVALID_HANDLE && handle == untouched, "set needs an initialized handle");
    Require(sceSrcUtilityGetPriority(handle.data(), &priority) == INVALID_HANDLE && priority == 7, "get needs an initialized handle");
    Require(sceSrcUtilityGetPriority(handle.data(), nullptr) == INVALID_HANDLE, "the handle is checked before the output");
    std::memcpy(handle.data() + 0x9470, &MAGIC, sizeof(MAGIC));
    std::vector<std::uint8_t> expected = handle;
    Require(sceSrcUtilityGetPriority(handle.data(), nullptr) == INVALID_POINTER, "get needs an output");
    Require(sceSrcUtilitySetPriority(handle.data(), 0x12345678) == 0, "set");
    const std::uint32_t stored = 0x12345678;
    std::memcpy(expected.data() + 0x2C, &stored, sizeof(stored));
    Require(handle == expected, "set writes only the priority field");
    Require(sceSrcUtilityGetPriority(handle.data(), &priority) == 0 && priority == 0x12345678, "get");
    Require(handle == expected, "get leaves the handle unchanged");
    std::uint64_t other = MAGIC ^ 1;
    std::memcpy(handle.data() + 0x9470, &other, sizeof(other));
    priority = 7;
    Require(sceSrcUtilityGetPriority(handle.data(), &priority) == INVALID_HANDLE && priority == 7, "a changed magic is rejected");
}

void CheckInitializeFinalize() {
    Require(sceSrcUtilityFinalize(nullptr) == INVALID_HANDLE, "finalize on a null handle");
    std::vector<std::uint8_t> handle(HANDLE_SIZE, 0x5A);
    const std::vector<std::uint8_t> untouched = handle;
    Require(sceSrcUtilityFinalize(handle.data()) == INVALID_HANDLE, "finalize on an uninitialized handle");
    Require(handle == untouched, "finalize leaves an uninitialized handle unchanged");

    Require(sceSrcUtilityInitialize(nullptr, 2, 0, 48000, 48000) == INVALID_POINTER, "initialize on a null handle");
    Require(sceSrcUtilityInitialize(handle.data(), 0, 0, 48000, 48000) == INVALID_CHANNELS, "initialize with 0 channels");
    Require(handle == untouched, "rejected initialize leaves handle unchanged");
    Require(sceSrcUtilityInitialize(handle.data(), 9, 0, 48000, 48000) == INVALID_CHANNELS, "initialize with 9 channels");
    Require(handle == untouched, "rejected initialize leaves handle unchanged");
    Require(sceSrcUtilityInitialize(handle.data(), 2, 3, 48000, 48000) == INVALID_MODE, "initialize with invalid mode");
    Require(handle == untouched, "rejected initialize leaves handle unchanged");
    Require(sceSrcUtilityInitialize(handle.data(), 2, 0, 0, 48000) == INVALID_INPUT_RATE, "initialize with invalid input rate");
    Require(handle == untouched, "rejected initialize leaves handle unchanged");
    Require(sceSrcUtilityInitialize(handle.data(), 2, 0, 48000, 0) == INVALID_OUTPUT_RATE, "initialize with invalid output rate");
    Require(handle == untouched, "rejected initialize leaves handle unchanged");

    Require(sceSrcUtilityInitialize(handle.data(), 2, 0, 48000, 48000) == 0, "valid initialize");

    std::uint64_t magic = 0;
    std::memcpy(&magic, handle.data() + 0x9470, sizeof(magic));
    Require(magic == MAGIC, "initialize sets magic");

    std::uint32_t priority = 0;
    std::memcpy(&priority, handle.data() + 0x2C, sizeof(priority));
    Require(priority == 0x28, "initialize sets default priority");

    std::vector<std::uint8_t> expected(HANDLE_SIZE, 0);
    const std::uint32_t defaultPriority = 0x28;
    std::memcpy(expected.data() + 0x2C, &defaultPriority, sizeof(defaultPriority));
    std::memcpy(expected.data() + 0x9470, &MAGIC, sizeof(MAGIC));
    Require(handle == expected, "initialize clears handle and sets priority and magic");

    std::uint32_t queriedPriority = 0;
    Require(sceSrcUtilityGetPriority(handle.data(), &queriedPriority) == 0 && queriedPriority == 0x28, "get priority returns default priority");

    Require(sceSrcUtilityFinalize(handle.data()) == 0, "finalize on initialized handle");
    std::memcpy(&magic, handle.data() + 0x9470, sizeof(magic));
    Require(magic == 0, "finalize clears magic");

    Require(sceSrcUtilityFinalize(handle.data()) == INVALID_HANDLE, "second finalize on already finalized handle");
    Require(sceSrcUtilityGetPriority(handle.data(), &queriedPriority) == INVALID_HANDLE, "get priority rejected after finalize");
}

}

int main() {
    CheckHandleSize();
    CheckPriority();
    CheckInitializeFinalize();
}

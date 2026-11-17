#include "prx/libSceSrcUtl/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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
    Require(sceSrcUtilitySetPriority(reinterpret_cast<void*>(0x10), 1) == INVALID_HANDLE, "set on an invalid handle pointer");
    Require(sceSrcUtilityGetPriority(reinterpret_cast<const void*>(0x10), &priority) == INVALID_HANDLE && priority == 7, "get on an invalid handle pointer");
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

void CheckUnwritablePriorityOutput() {
    constexpr std::size_t kRegionSize = 0xA000;
    std::uint8_t* page = nullptr;
#ifdef _WIN32
    page = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, kRegionSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    void* mapped = mmap(nullptr, kRegionSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    page = mapped == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(mapped);
#endif
    Require(page != nullptr, "a mapped region must be available");
    std::memcpy(page + 0x9470, &MAGIC, sizeof(MAGIC));
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, 4096, PAGE_NOACCESS, &oldProtect) != 0, "first page must become no-access");
#else
    Require(mprotect(page, 4096, PROT_NONE) == 0, "first page must become no-access");
#endif
    std::vector<std::uint8_t> handle(HANDLE_SIZE, 0);
    std::memcpy(handle.data() + 0x9470, &MAGIC, sizeof(MAGIC));
    const std::uint32_t stored = 0x0BADF00D;
    std::memcpy(handle.data() + 0x2C, &stored, sizeof(stored));
    errno = 71;
    Require(sceSrcUtilityGetPriority(handle.data(), reinterpret_cast<std::uint32_t*>(page)) == INVALID_POINTER && errno == 71,
            "get must report an unwritable output as the invalid pointer error");
    std::uint32_t outputPriority = 9;
    Require(sceSrcUtilityGetPriority(page, &outputPriority) == INVALID_HANDLE && outputPriority == 9 && errno == 71,
            "get must report an unreadable priority field as invalid handle");
    Require(sceSrcUtilitySetPriority(page, 1) == INVALID_HANDLE && errno == 71,
            "set must report an unwritable handle as invalid handle");
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, kRegionSize);
#endif
}

void CheckEarlyAnswers() {
    constexpr int FLUSHED = static_cast<int>(0x815D000A);
    std::uint32_t context = 7;
    std::vector<std::uint8_t> handle(HANDLE_SIZE, 0xAB);
    Require(sceSrcUtilityInitializeEx(nullptr) == INVALID_POINTER, "initialize ex needs a context");
    Require(sceSrcUtilityCreateEx(nullptr, 0, 9, 0, 0, nullptr) == INVALID_POINTER, "create ex checks the context first");
    Require(sceSrcUtilityCreateEx(&context, 0, 9, 0, 0, nullptr) == INVALID_HANDLE, "create ex checks the handle second");
    Require(sceSrcUtilityInitialize(0, 9, 0, 0, nullptr) == INVALID_HANDLE, "initialize checks the handle first");
    const std::vector<std::uint8_t> untouched = handle;
    for (std::uint32_t channels = 0; channels <= 9; ++channels) {
        for (std::uint32_t mode = 0; mode <= 3; ++mode) {
            for (const std::uint32_t input : {0u, 22050u, 48000u, 384000u}) {
                for (const std::uint32_t output : {0u, 44100u, 192000u, 22050u}) {
                    const int expected = sceSrcUtilityGetHandleSize(channels, mode, input, output);
                    if (expected == HANDLE_SIZE) continue;
                    Require(sceSrcUtilityCreateEx(&context, channels, mode, input, output, handle.data()) == expected, "create ex checks like the handle size");
                    Require(sceSrcUtilityInitialize(channels, mode, input, output, handle.data()) == expected, "initialize checks like the handle size");
                }
            }
        }
    }
    Require(handle == untouched && context == 7, "failed checks write nothing");

    for (void* bad : {static_cast<void*>(nullptr), static_cast<void*>(handle.data())}) {
        Require(sceSrcUtilityFinalize(bad) == INVALID_HANDLE, "finalize needs an initialized handle");
        Require(sceSrcUtilityDeleteEx(bad) == INVALID_HANDLE, "delete ex needs an initialized handle");
        Require(sceSrcUtilityFlush(bad, -1, nullptr, nullptr) == INVALID_HANDLE, "flush checks the handle first");
        Require(sceSrcUtilityResample(bad, -1) == INVALID_HANDLE, "resample checks the handle first");
    }
    std::memcpy(handle.data() + 0x9470, &MAGIC, sizeof(MAGIC));
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    Require(sceSrcUtilityFlush(handle.data(), -1, &first, &second) == INVALID_POINTER, "flush refuses a negative count");
    Require(sceSrcUtilityFlush(handle.data(), 0, nullptr, &second) == INVALID_POINTER, "flush needs its first pointer");
    Require(sceSrcUtilityFlush(handle.data(), 0, &first, nullptr) == INVALID_POINTER, "flush needs its second pointer");
    const std::uint32_t flushed = 2;
    std::memcpy(handle.data() + 0x30, &flushed, sizeof(flushed));
    Require(sceSrcUtilityFlush(handle.data(), 0, &first, &second) == FLUSHED, "flush after a flush");
    Require(sceSrcUtilityResample(handle.data(), -1) == INVALID_POINTER, "resample refuses a negative count");
    Require(first == 0 && second == 0, "flush checks write nothing");

    Require(sceSrcUtilityGetAjmContext(nullptr, &context) == INVALID_HANDLE && context == 7, "the context needs a handle");
    Require(sceSrcUtilityGetAjmContext(handle.data(), nullptr) == 0, "a null output is accepted");
    const std::uint32_t stored = 0x13572468;
    std::memcpy(handle.data() + 0x9478, &stored, sizeof(stored));
    Require(sceSrcUtilityGetAjmContext(handle.data(), &context) == 0 && context == stored, "the context is read from the handle");
    std::vector<std::uint8_t> plain(HANDLE_SIZE, 0);
    std::memcpy(plain.data() + 0x9478, &stored, sizeof(stored));
    context = 7;
    Require(sceSrcUtilityGetAjmContext(plain.data(), &context) == 0 && context == stored, "the magic is not checked");
}

}

int main() {
    CheckHandleSize();
    CheckPriority();
    CheckUnwritablePriorityOutput();
    CheckEarlyAnswers();
}

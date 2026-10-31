#include "prx/libSceNpProfileDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005u);
constexpr int kErrArgNull = static_cast<int>(0x80B8000Du);
constexpr std::int32_t kResultUserCanceled = 1;

constexpr std::size_t kParamSize = 128;
constexpr std::size_t kParamASize = 120;
constexpr std::size_t kParamUserData = 88;
constexpr std::size_t kParamAUserData = 80;
constexpr std::size_t kResultSize = 48;
constexpr std::uint8_t kSentinel = 0x5a;

template <typename TValue>
void Store(std::uint8_t* bytes, std::size_t offset, TValue value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}

template <typename TValue>
TValue Load(const std::uint8_t* bytes, std::size_t offset) {
    TValue value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

void MakeParam(std::uint8_t* bytes, std::size_t capacity, std::uint64_t size, std::uint32_t mode, std::size_t userDataOffset, void* userData) {
    std::memset(bytes, 0, capacity);
    Store<std::uint64_t>(bytes, 0, 48);
    Store<std::uint64_t>(bytes, 48, size);
    Store<std::uint32_t>(bytes, 56, mode);
    Store<std::int32_t>(bytes, 60, 1);
    Store<void*>(bytes, userDataOffset, userData);
}

template <typename TCall>
void RequireThrows(TCall call) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("not implemented") == std::string::npos;
    }
    Require(threw);
}

void RequireCanceled(const std::uint8_t* result, void* userData) {
    Require(Load<std::int32_t>(result, 0) == 0);
    Require(Load<std::int32_t>(result, 4) == kResultUserCanceled);
    Require(Load<void*>(result, 8) == userData);
    for (std::size_t i = 16; i < kResultSize; ++i) Require(result[i] == 0);
    for (std::size_t i = kResultSize; i < kResultSize + 16; ++i) Require(result[i] == kSentinel);
}

}

int main() {
    alignas(8) std::uint8_t param[kParamSize + 16];
    alignas(8) std::uint8_t paramA[kParamASize + 16];
    alignas(8) std::uint8_t result[kResultSize + 16];
    int first = 0;
    int second = 0;
    std::memset(result, kSentinel, sizeof(result));
    MakeParam(param, sizeof(param), kParamSize, 1, kParamUserData, &first);
    MakeParam(paramA, sizeof(paramA), kParamASize, 1, kParamAUserData, &second);

    Require(sceNpProfileDialogTerminate() == kErrNotInitialized);
    Require(sceNpProfileDialogClose() == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpProfileDialogGetStatus() == kStatusNone);
    Require(sceNpProfileDialogUpdateStatus() == kStatusNone);
    RequireThrows([&] { sceNpProfileDialogOpen(reinterpret_cast<const SceNpProfileDialogParam*>(param)); });
    RequireThrows([&] { sceNpProfileDialogOpenA(reinterpret_cast<const SceNpProfileDialogParamA*>(paramA)); });
    Require(sceNpProfileDialogGetStatus() == kStatusNone);
    Require(result[0] == kSentinel);

    Require(sceNpProfileDialogInitialize() == 0);
    Require(sceNpProfileDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpProfileDialogGetStatus() == kStatusInitialized);
    Require(sceNpProfileDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotFinished);
    Require(result[0] == kSentinel);
    RequireThrows([] { sceNpProfileDialogGetResult(nullptr); });
    RequireThrows([] { sceNpProfileDialogClose(); });
    Require(sceNpProfileDialogGetStatus() == kStatusInitialized);

    Require(sceNpProfileDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpProfileDialogOpenA(nullptr) == kErrArgNull);
    Require(sceNpProfileDialogGetStatus() == kStatusInitialized);

    alignas(8) std::uint8_t wrong[kParamSize + 16];
    MakeParam(wrong, sizeof(wrong), kParamASize, 1, kParamUserData, &first);
    RequireThrows([&] { sceNpProfileDialogOpen(reinterpret_cast<const SceNpProfileDialogParam*>(wrong)); });
    MakeParam(wrong, sizeof(wrong), kParamSize, 1, kParamAUserData, &second);
    RequireThrows([&] { sceNpProfileDialogOpenA(reinterpret_cast<const SceNpProfileDialogParamA*>(wrong)); });
    MakeParam(wrong, sizeof(wrong), kParamSize, 0, kParamUserData, &first);
    RequireThrows([&] { sceNpProfileDialogOpen(reinterpret_cast<const SceNpProfileDialogParam*>(wrong)); });
    MakeParam(wrong, sizeof(wrong), kParamASize, 5, kParamAUserData, &second);
    RequireThrows([&] { sceNpProfileDialogOpenA(reinterpret_cast<const SceNpProfileDialogParamA*>(wrong)); });
    Require(sceNpProfileDialogGetStatus() == kStatusInitialized);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotFinished);

    std::uint8_t paramCopy[sizeof(param)];
    std::memcpy(paramCopy, param, sizeof(param));
    Require(sceNpProfileDialogOpen(reinterpret_cast<const SceNpProfileDialogParam*>(param)) == 0);
    Require(std::memcmp(param, paramCopy, sizeof(param)) == 0);
    Require(sceNpProfileDialogGetStatus() == kStatusFinished);
    Require(sceNpProfileDialogUpdateStatus() == kStatusFinished);
    Require(sceNpProfileDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == 0);
    RequireCanceled(result, &first);
    RequireThrows([] { sceNpProfileDialogClose(); });
    Require(sceNpProfileDialogGetStatus() == kStatusFinished);

    for (std::uint32_t mode = 1; mode <= 4; ++mode) {
        Store<std::uint32_t>(paramA, 56, mode);
        std::uint8_t paramACopy[sizeof(paramA)];
        std::memcpy(paramACopy, paramA, sizeof(paramA));
        std::memset(result, kSentinel, sizeof(result));
        Require(sceNpProfileDialogOpenA(reinterpret_cast<const SceNpProfileDialogParamA*>(paramA)) == 0);
        Require(std::memcmp(paramA, paramACopy, sizeof(paramA)) == 0);
        Require(sceNpProfileDialogUpdateStatus() == kStatusFinished);
        Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == 0);
        RequireCanceled(result, &second);
    }

    Require(sceNpProfileDialogTerminate() == 0);
    Require(sceNpProfileDialogTerminate() == kErrNotInitialized);
    Require(sceNpProfileDialogClose() == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpProfileDialogGetStatus() == kStatusNone);
    Require(sceNpProfileDialogUpdateStatus() == kStatusNone);

    Require(sceNpProfileDialogInitialize() == 0);
    std::memset(result, kSentinel, sizeof(result));
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotFinished);
    Require(result[0] == kSentinel);
    Store<std::uint32_t>(param, 56, 4);
    Require(sceNpProfileDialogOpen(reinterpret_cast<const SceNpProfileDialogParam*>(param)) == 0);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == 0);
    RequireCanceled(result, &first);
    Require(sceNpProfileDialogTerminate() == 0);
    Require(sceNpProfileDialogTerminate() == kErrNotInitialized);
    Require(sceNpProfileDialogClose() == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(reinterpret_cast<SceNpProfileDialogResult*>(result)) == kErrNotInitialized);
    Require(sceNpProfileDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpProfileDialogGetStatus() == kStatusNone);
    Require(sceNpProfileDialogUpdateStatus() == kStatusNone);
    return 0;
}

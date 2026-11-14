#include "prx/libSceSystemLogger2/libSceSystemLogger2.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int kInvalidArgument = static_cast<int>(0x818C0002u);
constexpr int kNotInitialized = static_cast<int>(0x818C0004u);
constexpr int kNotAvailable = static_cast<int>(0x818C000Eu);
constexpr std::uint64_t kUnixEpochTick = 62135596800000000ull;
constexpr std::size_t kRecordSize = 0x1000;
constexpr std::size_t kClockOffset = 0x90;
constexpr std::size_t kCountOffset = 0xB8;
constexpr std::size_t kEntryOffset = 0xD0;
constexpr std::size_t kEntryLanguageOffset = 0xD8;
constexpr std::size_t kDetailOffset = 0xCC;
constexpr std::size_t kExNameOffset = 0x168;
constexpr std::size_t kEx2NameOffset = 0x4D0;
constexpr std::size_t kRnpsTokenOffset = 0x158;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "libSceSystemLogger2 check failed at line %u\n", location.line());
    std::abort();
}

void StoreU32(std::uint8_t* record, std::size_t offset, std::uint32_t value) {
    std::memcpy(record + offset, &value, sizeof(value));
}

void StorePointer(std::uint8_t* record, std::size_t offset, const void* value) {
    std::memcpy(record + offset, &value, sizeof(value));
}

void SetClockSource(std::uint8_t* record, const char* source) {
    std::memcpy(record + kClockOffset, source, std::strlen(source) + 1);
}

void FillName(char* name, std::size_t length) {
    std::memset(name, 'a', length);
    name[length] = '\0';
}

}

int main() {
    alignas(16) std::uint8_t record[kRecordSize] = {};
    SceSystemLogger2Timestamp timestamp{};
    std::uint32_t eventStatus = 0xDEADBEEF;
    std::uint64_t eventState = 0xDEADBEEFDEADBEEFull;

    Require(sizeof(SceSystemLogger2Timestamp) == 0x20);
    Require(offsetof(SceSystemLogger2Timestamp, clockSource) == 8);

    Require(sceSystemLogger2GetTimestamp(&timestamp) == kNotInitialized);
    Require(sceSystemLogger2SetEventAsync(nullptr) == kNotInitialized);
    Require(sceSystemLogger2TerminateWithResourceRelease() == kNotInitialized);

    Require(sceSystemLogger2Initialize() == 0);
    Require(sceSystemLogger2Initialize() == 0);

    Require(sceSystemLogger2GetTimestamp(nullptr) == kInvalidArgument);
    std::memset(&timestamp, 0xAA, sizeof(timestamp));
    Require(sceSystemLogger2GetTimestamp(&timestamp) == 0);
    Require(timestamp.tick >= kUnixEpochTick);
    Require(std::strcmp(timestamp.clockSource, "networkClock") == 0);
    for (std::size_t index = std::strlen("networkClock") + 1; index < sizeof(timestamp.clockSource); ++index) {
        Require(timestamp.clockSource[index] == 0);
    }

    Require(sceSystemLogger2IsAvailableMonitor() == kNotAvailable);

    char statusName[0x26] = {};
    FillName(statusName, 0x24);
    Require(sceSystemLogger2GetEventStatus(statusName, &eventState) == kNotAvailable);
    Require(eventState == 0);
    FillName(statusName, 0x25);
    Require(sceSystemLogger2GetEventStatus(statusName, &eventState) == kInvalidArgument);
    Require(sceSystemLogger2GetEventStatus("", &eventState) == kInvalidArgument);
    Require(sceSystemLogger2GetEventStatus(nullptr, &eventState) == kInvalidArgument);
    Require(sceSystemLogger2GetEventStatus("name", nullptr) == kInvalidArgument);

    Require(sceSystemLogger2SetEventAsyncInternal(nullptr, &eventStatus) == kInvalidArgument);
    Require(sceSystemLogger2SetEventAsyncInternal(record, nullptr) == kInvalidArgument);
    eventStatus = 0xDEADBEEF;
    Require(sceSystemLogger2SetEventAsyncInternal(record, &eventStatus) == kInvalidArgument);
    Require(eventStatus == 0xDEADBEEF);

    SetClockSource(record, "NONE");
    Require(sceSystemLogger2SetEventAsyncInternal(record, &eventStatus) == kNotAvailable);
    Require(eventStatus == 0);
    Require(sceSystemLogger2SetEventAsync(record) == kNotAvailable);

    char longName[0x82];
    FillName(longName, 0x81);
    std::memcpy(record, longName, 0x81);
    Require(sceSystemLogger2SetEventAsyncInternal(record, &eventStatus) == kInvalidArgument);
    std::memset(record, 0, 0x81);
    Require(sceSystemLogger2SetEventAsyncInternal(record, &eventStatus) == kNotAvailable);

    StoreU32(record, kCountOffset, 5);
    Require(sceSystemLogger2SetEventAsyncInternal(record, &eventStatus) == kInvalidArgument);
    StoreU32(record, kCountOffset, 0);

    Require(sceSystemLogger2SetEventAsyncEx(nullptr) == kInvalidArgument);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kNotAvailable);
    StoreU32(record, kCountOffset, 1);
    StorePointer(record, kEntryOffset, longName);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kInvalidArgument);
    StorePointer(record, kEntryOffset, nullptr);
    char language[4];
    FillName(language, 3);
    StorePointer(record, kEntryLanguageOffset, language);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kInvalidArgument);
    StorePointer(record, kEntryLanguageOffset, nullptr);
    StoreU32(record, kCountOffset, 0);

    char exName[0x82];
    FillName(exName, 0x81);
    std::memcpy(record + kExNameOffset, exName, 0x81);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kInvalidArgument);
    std::memset(record + kExNameOffset, 0, 0x81);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kNotAvailable);

    Require(sceSystemLogger2SetEventAsyncEx2(nullptr) == kInvalidArgument);
    Require(sceSystemLogger2SetEventAsyncEx2(record) == kNotAvailable);
    char detail[0x402];
    FillName(detail, 0x401);
    std::memcpy(record + kDetailOffset, detail, 0x401);
    Require(sceSystemLogger2SetEventAsyncEx2(record) == kInvalidArgument);
    std::memset(record + kDetailOffset, 0, 0x401);
    std::memcpy(record + kEx2NameOffset, exName, 0x81);
    Require(sceSystemLogger2SetEventAsyncEx2(record) == kInvalidArgument);
    std::memset(record + kEx2NameOffset, 0, 0x81);
    Require(sceSystemLogger2SetEventAsyncEx2(record) == kNotAvailable);

    Require(sceSystemLogger2SetRnpsEventAsync(nullptr) == kInvalidArgument);
    Require(sceSystemLogger2SetRnpsEventAsync(record) == kInvalidArgument);
    StorePointer(record, kRnpsTokenOffset, record);
    Require(sceSystemLogger2SetRnpsEventAsync(record) == kNotAvailable);

    errno = 71;
    eventStatus = 0xDEADBEEF;
    const auto* unreadable = reinterpret_cast<const void*>(0x10);
    Require(sceSystemLogger2SetEventAsyncInternal(unreadable, &eventStatus) == kInvalidArgument);
    Require(eventStatus == 0xDEADBEEF);
    Require(sceSystemLogger2SetEventAsync(unreadable) == kInvalidArgument);
    Require(sceSystemLogger2SetEventAsyncEx(unreadable) == kInvalidArgument);
    Require(sceSystemLogger2SetEventAsyncEx2(unreadable) == kInvalidArgument);
    Require(sceSystemLogger2SetRnpsEventAsync(unreadable) == kInvalidArgument);
    Require(errno == 71);

    StoreU32(record, kCountOffset, 1);
    StorePointer(record, kEntryOffset, unreadable);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kInvalidArgument);
    StorePointer(record, kEntryOffset, nullptr);
    StorePointer(record, kEntryLanguageOffset, unreadable);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kInvalidArgument);
    StorePointer(record, kEntryLanguageOffset, nullptr);
    StoreU32(record, kCountOffset, 0);
    Require(errno == 71);
    Require(sceSystemLogger2SetEventAsyncEx(record) == kNotAvailable);

    Require(sceSystemLogger2Terminate() == 0);
    Require(sceSystemLogger2GetTimestamp(&timestamp) == 0);
    Require(sceSystemLogger2TerminateWithResourceRelease() == 0);
    Require(sceSystemLogger2GetTimestamp(&timestamp) == kNotInitialized);
    Require(sceSystemLogger2TerminateWithResourceRelease() == kNotInitialized);

    Require(sceSystemLogger2Initialize() == 0);
    Require(sceSystemLogger2GetTimestamp(&timestamp) == 0);
    return 0;
}

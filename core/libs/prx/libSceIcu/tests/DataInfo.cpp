#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/udata.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

constexpr bool HostBigEndian = std::endian::native == std::endian::big;

struct GuestDataMemory {
    const void* functions;
    const void* header;
    const void* contents;
    std::int32_t heapAllocated;
    std::int32_t padding;
    void* mapAddress;
    void* map;
    std::int32_t length;
    std::int32_t trailingPadding;
};

struct HeaderStorage {
    std::uint16_t headerSize;
    std::uint8_t magic1;
    std::uint8_t magic2;
    std::array<std::uint8_t, 64> info;
};

struct GuardedInfo {
    std::array<std::uint8_t, 16> before;
    alignas(Icu::DataInfo) std::array<std::uint8_t, 64> info;
    std::array<std::uint8_t, 16> after;
};

static_assert(sizeof(Icu::DataInfo) == 20);
static_assert(alignof(Icu::DataInfo) == 2);
static_assert(sizeof(GuestDataMemory) == 56);
static_assert(alignof(GuestDataMemory) == 8);
static_assert(offsetof(GuestDataMemory, header) == 8);
static_assert(offsetof(HeaderStorage, info) == 4);
static_assert(std::is_same_v<decltype(&udata_getInfo_nid_postfix), void (APS5_VABI*)(Icu::DataMemory*, Icu::DataInfo*)>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU data info: %s\n", message);
    std::abort();
}

void Store16(std::uint8_t* destination, std::uint16_t value, bool bigEndian) {
    destination[bigEndian ? 1 : 0] = static_cast<std::uint8_t>(value);
    destination[bigEndian ? 0 : 1] = static_cast<std::uint8_t>(value >> 8);
}

std::uint16_t Load16(const std::uint8_t* source, bool bigEndian) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(source[bigEndian ? 0 : 1]) << 8)
        | source[bigEndian ? 1 : 0]);
}

HeaderStorage MakeGuestHeader(std::uint16_t size, bool bigEndian) {
    HeaderStorage header{};
    header.headerSize = 24;
    header.magic1 = 0xda;
    header.magic2 = 0x27;
    for (std::size_t index = 0; index < header.info.size(); ++index)
        header.info[index] = static_cast<std::uint8_t>(0x31 + index * 7);
    Store16(header.info.data(), size, bigEndian);
    Store16(header.info.data() + 2, 0x39a7, bigEndian);
    header.info[4] = bigEndian;
    header.info[5] = 1;
    header.info[6] = 2;
    header.info[7] = 0;
    return header;
}

GuardedInfo MakeDestination(std::uint16_t size) {
    GuardedInfo destination;
    destination.before.fill(0xa5);
    destination.info.fill(0x6c);
    destination.after.fill(0x5a);
    Store16(destination.info.data(), size, HostBigEndian);
    return destination;
}

GuardedInfo Expected(const HeaderStorage& header, std::uint16_t requested) {
    GuardedInfo expected = MakeDestination(requested);
    const bool sourceBigEndian = header.info[4] != 0;
    const std::uint16_t sourceSize = Load16(header.info.data(), sourceBigEndian);
    const std::uint16_t copiedSize = requested > sourceSize ? sourceSize : requested;
    if (requested > sourceSize) Store16(expected.info.data(), sourceSize, HostBigEndian);
    std::memcpy(expected.info.data() + 2, header.info.data() + 2, copiedSize - 2);
    if (sourceBigEndian != HostBigEndian)
        Store16(expected.info.data() + 2, Load16(header.info.data() + 2, sourceBigEndian), HostBigEndian);
    return expected;
}

void CompareValid(std::uint16_t sourceSize, bool sourceBigEndian, std::uint16_t requested) {
    HeaderStorage header = MakeGuestHeader(sourceSize, sourceBigEndian);
    GuestDataMemory data{};
    data.header = &header;
    GuardedInfo actual = MakeDestination(requested);
    GuardedInfo reference = actual;
    errno = 73;
    udata_getInfo_nid_postfix(reinterpret_cast<Icu::DataMemory*>(&data), reinterpret_cast<Icu::DataInfo*>(actual.info.data()));
    Require(errno == 73, "wrapper preserves errno for a valid header");
    errno = 73;
    udata_getInfo(reinterpret_cast<UDataMemory*>(&data), reinterpret_cast<UDataInfo*>(reference.info.data()));
    Require(errno == 73, "bundled reference preserves errno for a valid header");
    Require(actual.before == std::array<std::uint8_t, 16>{0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5}, "wrapper preserves the leading guard");
    Require(actual.after == std::array<std::uint8_t, 16>{0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a}, "wrapper preserves the trailing guard");
    Require(std::memcmp(&actual, &reference, sizeof(actual)) == 0, "wrapper matches bundled ICU");
    const GuardedInfo expected = Expected(header, requested);
    Require(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "wrapper matches ICU 68 copy and byte-order semantics");
}

void CheckAbsentData(std::uint16_t requested) {
    for (bool nullData : {false, true}) {
        GuestDataMemory data{};
        GuardedInfo actual = MakeDestination(requested);
        GuardedInfo expected = actual;
        Store16(expected.info.data(), 0, HostBigEndian);
        errno = 73;
        udata_getInfo_nid_postfix(nullData ? nullptr : reinterpret_cast<Icu::DataMemory*>(&data), reinterpret_cast<Icu::DataInfo*>(actual.info.data()));
        Require(errno == 73, "absent data preserves errno");
        Require(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "absent data clears only the size");
    }
}

using OpenedData = std::unique_ptr<UDataMemory, decltype(&udata_close_nid_postfix)>;

void CheckOpenedData(UDataMemory* data) {
    Require(data != nullptr, "native data handle");
    Icu::DataInfo info{};
    info.size = sizeof(info);
    udata_getInfo_nid_postfix(reinterpret_cast<Icu::DataMemory*>(data), &info);
    Require(info.size == sizeof(info) && std::memcmp(info.dataFormat, "Nrm2", 4) == 0, "normalization metadata");
    Require(udata_getMemory(data) != nullptr, "normalization payload");
}

void CheckDataOpen() {
    std::int32_t status = U_ZERO_ERROR;
    OpenedData data(udata_open_nid_postfix(nullptr, "nrm", "nfkc", &status), udata_close_nid_postfix);
    Require(status == U_ZERO_ERROR, "open built-in data");
    CheckOpenedData(data.get());
    data.reset();

    status = U_USING_DEFAULT_WARNING;
    data.reset(udata_open_nid_postfix("ICUDATA", nullptr, "nfkc.nrm", &status));
    Require(status == U_USING_DEFAULT_WARNING, "open by full item name preserves warning");
    CheckOpenedData(data.get());

    for (const std::int32_t incoming : {std::int32_t{-129}, INT32_MIN}) {
        status = incoming;
        data.reset(udata_open_nid_postfix(nullptr, "nrm", "nfkc", &status));
        Require(status == incoming, "open keeps a status below the warning range");
        CheckOpenedData(data.get());
    }

    status = U_ZERO_ERROR;
    const std::string missingName(600, 'x');
    Require(!udata_open_nid_postfix(nullptr, "nrm", missingName.c_str(), &status)
            && status == U_FILE_ACCESS_ERROR, "missing item");

    const auto* unreadable = reinterpret_cast<const char*>(std::uintptr_t{1});
    Require(!udata_open_nid_postfix(unreadable, unreadable, unreadable, &status)
            && status == U_FILE_ACCESS_ERROR, "preexisting failure skips strings");
    Require(!udata_open_nid_postfix(unreadable, unreadable, unreadable, nullptr), "null status skips strings");

    status = U_ZERO_ERROR;
    Require(!udata_open_nid_postfix(unreadable, unreadable, "", &status)
            && status == U_ILLEGAL_ARGUMENT_ERROR, "empty name skips path and type");
}

struct ChoiceContext {
    const char* type;
    const char* name;
    bool accept;
    int calls = 0;
};

std::int8_t APS5_VABI SelectData(void* context, const char* type, const char* name, const Icu::DataInfo* info) {
    auto& choice = *static_cast<ChoiceContext*>(context);
    Require(type == choice.type && name == choice.name, "callback receives original guest strings");
    Require(info && info->size == sizeof(*info) && std::memcmp(info->dataFormat, "Nrm2", 4) == 0,
            "callback sees normalization metadata");
    ++choice.calls;
    return choice.accept;
}

std::int8_t APS5_VABI ThrowFromChoice(void*, const char*, const char*, const Icu::DataInfo*) {
    throw std::runtime_error("data filter failed");
}

void CheckDataChoice() {
    ChoiceContext choice{nullptr, "nfkc.nrm", true};
    std::int32_t status = U_ZERO_ERROR;
    OpenedData data(udata_openChoice_nid_postfix(nullptr, choice.type, choice.name, SelectData, &choice, &status),
                    udata_close_nid_postfix);
    Require(status == U_ZERO_ERROR && choice.calls == 1, "callback accepts data");
    CheckOpenedData(data.get());
    data.reset();

    choice.accept = false;
    Require(!udata_openChoice_nid_postfix(nullptr, choice.type, choice.name, SelectData, &choice, &status)
            && status == U_INVALID_FORMAT_ERROR && choice.calls > 1, "callback rejects data");

    status = U_ZERO_ERROR;
    Require(!udata_openChoice_nid_postfix(nullptr, choice.type, choice.name, nullptr, nullptr, &status)
            && status == U_ILLEGAL_ARGUMENT_ERROR, "null callback");

    status = U_ZERO_ERROR;
    bool caught = false;
    try {
        data.reset(udata_openChoice_nid_postfix(nullptr, choice.type, choice.name, ThrowFromChoice, nullptr, &status));
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Require(caught && status == U_ZERO_ERROR, "callback exception returns to the guest caller");
}

}

int main() {
    errno = 73;
    udata_getInfo_nid_postfix(reinterpret_cast<Icu::DataMemory*>(std::uintptr_t{1}), nullptr);
    Require(errno == 73, "null output ignores the data pointer and preserves errno");

    for (std::uint16_t requested : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{20}, std::uint16_t{0xffff}})
        CheckAbsentData(requested);

    constexpr std::array<std::uint16_t, 14> capacities{2, 3, 4, 5, 7, 8, 12, 19, 20, 21, 24, 31, 32, 40};
    for (bool sourceBigEndian : {false, true})
        for (std::uint16_t sourceSize : {std::uint16_t{20}, std::uint16_t{32}})
            for (std::uint16_t requested : capacities)
                CompareValid(sourceSize, sourceBigEndian, requested);

    CheckDataOpen();
    CheckDataChoice();
}

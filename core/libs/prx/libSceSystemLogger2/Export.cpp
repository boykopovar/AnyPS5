#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSystemLogger2/include/Api.hpp"
#include "prx/libSceSystemLogger2/include/InventoryExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

namespace {

constexpr int INVALID_ARGUMENT = static_cast<int>(0x818C0002u);
constexpr int NOT_INITIALIZED = static_cast<int>(0x818C0004u);
constexpr int NOT_AVAILABLE = static_cast<int>(0x818C000Eu);

constexpr std::size_t NAME_LIMIT = 0x80;
constexpr std::size_t EVENT_STATUS_NAME_LIMIT = 0x24;
constexpr std::uint32_t EVENT_ENTRY_LIMIT = 4;
constexpr std::size_t EVENT_ENTRY_STRIDE = 0x28;
constexpr std::size_t EVENT_ENTRY_NAME_LIMIT = 0x23;
constexpr std::size_t EVENT_ENTRY_LANGUAGE_LIMIT = 0x2;
constexpr std::size_t EVENT_DETAIL_LIMIT = 0x400;

constexpr std::size_t EVENT_NAME_OFFSET = 0x00;
constexpr std::size_t EVENT_CLOCK_OFFSET = 0x90;
constexpr std::size_t EVENT_COUNT_OFFSET = 0xB8;
constexpr std::size_t EVENT_ENTRY_OFFSET = 0xD0;
constexpr std::size_t EVENT_ENTRY_LANGUAGE_OFFSET = 0xD8;
constexpr std::size_t EVENT_DETAIL_OFFSET = 0xCC;
constexpr std::size_t EVENT_EX_NAME_OFFSET = 0x168;
constexpr std::size_t EVENT_EX2_NAME_OFFSET = 0x4D0;
constexpr std::size_t EVENT_RNPS_TOKEN_OFFSET = 0x158;

constexpr std::int64_t UNIX_EPOCH_DAYS = 719162;
constexpr std::int64_t TICKS_PER_DAY = 24LL * 60 * 60 * 1000000;
constexpr std::uint64_t UNIX_EPOCH_TICK = static_cast<std::uint64_t>(UNIX_EPOCH_DAYS * TICKS_PER_DAY);

std::mutex loggerMutex;
bool loggerInitialized = false;

std::uint64_t currentNetworkTick() {
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return UNIX_EPOCH_TICK + static_cast<std::uint64_t>(now);
}

bool fitsName(const char* name, std::size_t limit) {
    return strnlen(name, limit + 1) <= limit;
}

bool validClockSource(const char* source) {
    return std::memcmp(source, "networkClock", sizeof("networkClock")) == 0
        || std::memcmp(source, "secureClock", sizeof("secureClock")) == 0
        || std::memcmp(source, "NONE", sizeof("NONE")) == 0;
}

template <typename T>
T load(const std::uint8_t* record, std::size_t offset) {
    T value{};
    std::memcpy(&value, record + offset, sizeof(value));
    return value;
}

int validateEvent(const std::uint8_t* record) {
    const char* clockSource = reinterpret_cast<const char*>(record + EVENT_CLOCK_OFFSET);
    if (!validClockSource(clockSource)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_NAME_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_ENTRY_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    if (load<std::uint32_t>(record, EVENT_COUNT_OFFSET) > EVENT_ENTRY_LIMIT) return INVALID_ARGUMENT;
    return 0;
}

int validateEventEx(const std::uint8_t* record) {
    const char* clockSource = reinterpret_cast<const char*>(record + EVENT_CLOCK_OFFSET);
    if (!validClockSource(clockSource)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_NAME_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_EX_NAME_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    const std::uint32_t count = load<std::uint32_t>(record, EVENT_COUNT_OFFSET);
    if (count > EVENT_ENTRY_LIMIT) return INVALID_ARGUMENT;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t base = EVENT_ENTRY_OFFSET + index * EVENT_ENTRY_STRIDE;
        const char* name = load<const char*>(record, base);
        if (name && !fitsName(name, EVENT_ENTRY_NAME_LIMIT)) return INVALID_ARGUMENT;
        const char* language = load<const char*>(record, base + (EVENT_ENTRY_LANGUAGE_OFFSET - EVENT_ENTRY_OFFSET));
        if (language && !fitsName(language, EVENT_ENTRY_LANGUAGE_LIMIT)) return INVALID_ARGUMENT;
    }
    return 0;
}

int validateEventEx2(const std::uint8_t* record) {
    const char* clockSource = reinterpret_cast<const char*>(record + EVENT_CLOCK_OFFSET);
    if (!validClockSource(clockSource)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_NAME_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_EX2_NAME_OFFSET), NAME_LIMIT)) return INVALID_ARGUMENT;
    if (load<std::uint32_t>(record, EVENT_COUNT_OFFSET) > EVENT_ENTRY_LIMIT) return INVALID_ARGUMENT;
    if (!fitsName(reinterpret_cast<const char*>(record + EVENT_DETAIL_OFFSET), EVENT_DETAIL_LIMIT)) return INVALID_ARGUMENT;
    return 0;
}

} // namespace

extern "C" {

int APS5_VABI sceSystemLogger2Initialize(void) {
    std::lock_guard lock(loggerMutex);
    loggerInitialized = true;
    return 0;
}

int APS5_VABI sceSystemLogger2Terminate(void) {
    return 0;
}

int APS5_VABI sceSystemLogger2TerminateWithResourceRelease(void) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    loggerInitialized = false;
    return 0;
}

int APS5_VABI sceSystemLogger2GetTimestamp(SceSystemLogger2Timestamp* timestamp) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    if (!timestamp) return INVALID_ARGUMENT;
    SceSystemLogger2Timestamp value{};
    value.tick = currentNetworkTick();
    std::memcpy(value.clockSource, "networkClock", sizeof("networkClock"));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(timestamp, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceSystemLogger2IsAvailableMonitor(void) {
    std::lock_guard lock(loggerMutex);
    return NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2GetEventStatus(const char* eventName, std::uint64_t* status) {
    std::lock_guard lock(loggerMutex);
    if (!eventName || !status) return INVALID_ARGUMENT;
    std::string name;
    if (CopyGuestString_nid_no_patch(eventName, EVENT_STATUS_NAME_LIMIT + 1, name) != 0 || name.empty()) {
        return INVALID_ARGUMENT;
    }
    const std::uint64_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return INVALID_ARGUMENT;
    return NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2SetEventAsyncInternal(const void* event, std::uint32_t* status) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    if (!event || !status) return INVALID_ARGUMENT;
    const int result = validateEvent(static_cast<const std::uint8_t*>(event));
    if (result != 0) return result;
    const std::uint32_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return INVALID_ARGUMENT;
    return NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2SetEventAsync(const void* event) {
    std::uint32_t status = 0;
    return sceSystemLogger2SetEventAsyncInternal(event, &status);
}

int APS5_VABI sceSystemLogger2SetEventAsyncEx(const void* event) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    if (!event) return INVALID_ARGUMENT;
    const int result = validateEventEx(static_cast<const std::uint8_t*>(event));
    if (result != 0) return result;
    return NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2SetEventAsyncEx2(const void* event) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    if (!event) return INVALID_ARGUMENT;
    const int result = validateEventEx2(static_cast<const std::uint8_t*>(event));
    if (result != 0) return result;
    return NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2SetRnpsEventAsync(const void* event) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return NOT_INITIALIZED;
    if (!event) return INVALID_ARGUMENT;
    if (load<std::uintptr_t>(static_cast<const std::uint8_t*>(event), EVENT_RNPS_TOKEN_OFFSET) == 0) {
        return INVALID_ARGUMENT;
    }
    const int result = validateEvent(static_cast<const std::uint8_t*>(event));
    if (result != 0) return result;
    return NOT_AVAILABLE;
}

int APS5_VABI libSceSystemLogger2Unknown_PcjxaaFQATU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PcjxaaFQATU", libSceSystemLogger2Unknown_PcjxaaFQATU_nid_no_patch);

int APS5_VABI libSceSystemLogger2Unknown_UI7zoVuRjdU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UI7zoVuRjdU", libSceSystemLogger2Unknown_UI7zoVuRjdU_nid_no_patch);

int APS5_VABI libSceSystemLogger2Unknown_eI4wpp_plus_8Bco_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eI4wpp+8Bco", libSceSystemLogger2Unknown_eI4wpp_plus_8Bco_nid_no_patch);

}

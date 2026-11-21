#include "prx/libSceSystemService/include/SystemService.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace {

constexpr int AppMessagingInvalidArgument = static_cast<int>(0x80C50001);
constexpr int AppMessagingNullPointer = static_cast<int>(0x80C50004);
constexpr int AppMessagingNoMessage = static_cast<int>(0x80C50005);

std::mutex queueMutex;
std::condition_variable queueReady;
std::deque<AppMessagingMessage> queue;
std::mutex flagMutex;
std::uint64_t eventFlags;

int Validate(const void* data, std::size_t size) {
    if (size > sizeof(AppMessagingMessage::payload) || (size != 0 && !data)) return AppMessagingInvalidArgument;
    return 0;
}

}

extern "C" {

int APS5_VABI sceAppMessagingClearEventFlag(std::int32_t, std::uint64_t flags) {
    std::lock_guard lock(flagMutex);
    eventFlags &= ~flags;
    return 0;
}

int APS5_VABI sceAppMessagingReceiveMsg(AppMessagingMessage* message) {
    if (!message) return AppMessagingNullPointer;
    std::unique_lock lock(queueMutex);
    queueReady.wait(lock, [] { return !queue.empty(); });
    *message = queue.front();
    queue.pop_front();
    return 0;
}

int APS5_VABI sceAppMessagingSendMsg(std::uint32_t, std::uint32_t, const void* data, std::size_t size, std::uint32_t) {
    return Validate(data, size);
}

int APS5_VABI sceAppMessagingSendMsgToShellCore(std::uint32_t, const void* data, std::size_t size, std::uint32_t) {
    return Validate(data, size);
}

int APS5_VABI sceAppMessagingSendMsgToShellUI(std::uint32_t, const void* data, std::size_t size, std::uint32_t) {
    return Validate(data, size);
}

int APS5_VABI sceAppMessagingSetEventFlag(std::int32_t, std::uint64_t flags) {
    std::lock_guard lock(flagMutex);
    eventFlags |= flags;
    return 0;
}

int APS5_VABI sceAppMessagingTryGetEventFlag(std::uint64_t mask, std::uint64_t* flags, std::uint8_t clear) {
    if (!flags) return AppMessagingNullPointer;
    std::lock_guard lock(flagMutex);
    const std::uint64_t value = eventFlags & mask;
    *flags = value;
    if (clear) eventFlags &= ~value;
    return 0;
}

int APS5_VABI sceAppMessagingTryReceiveMsg(AppMessagingMessage* message) {
    if (!message) return AppMessagingNullPointer;
    std::lock_guard lock(queueMutex);
    if (queue.empty()) return AppMessagingNoMessage;
    *message = queue.front();
    queue.pop_front();
    return 0;
}

}

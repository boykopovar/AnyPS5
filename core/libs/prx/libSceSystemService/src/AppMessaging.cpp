#include "prx/libSceSystemService/include/SystemService.hpp"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

namespace {

constexpr int AppMessagingInvalidArgument = static_cast<int>(0x80C50001);
constexpr int AppMessagingNullPointer = static_cast<int>(0x80C50004);
constexpr int AppMessagingNoMessage = static_cast<int>(0x80C50005);
constexpr int ShellSender = 0;

std::mutex queueMutex;
std::condition_variable queueReady;
std::deque<AppMessagingMessage> queue;
std::mutex flagMutex;
std::uint64_t eventFlags;

std::uint64_t Now() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

int Enqueue(std::int32_t sender, std::uint32_t messageType, const void* data, std::size_t size, std::uint32_t) {
    if (size > sizeof(AppMessagingMessage::payload) || (size != 0 && !data)) return AppMessagingInvalidArgument;
    AppMessagingMessage message{};
    message.sender = sender;
    message.messageType = messageType;
    message.payloadSize = static_cast<std::uint32_t>(size);
    if (size != 0) std::memcpy(message.payload, data, size);
    message.timestamp = Now();
    {
        std::lock_guard lock(queueMutex);
        queue.push_back(message);
    }
    queueReady.notify_one();
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

int APS5_VABI sceAppMessagingSendMsg(std::uint32_t appId, std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    return Enqueue(static_cast<std::int32_t>(appId), messageId, data, size, flags);
}

int APS5_VABI sceAppMessagingSendMsgToShellCore(std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    return Enqueue(ShellSender, messageId, data, size, flags);
}

int APS5_VABI sceAppMessagingSendMsgToShellUI(std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    return Enqueue(ShellSender, messageId, data, size, flags);
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

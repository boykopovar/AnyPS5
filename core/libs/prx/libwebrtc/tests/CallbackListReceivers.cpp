#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc CallbackListReceivers check failed at line %u\n", location.line());
    std::abort();
}

struct GuardedReceivers {
    std::array<unsigned char, 16> before;
    WebRtcCallbackListReceivers object;
    std::array<unsigned char, 16> after;
};

static_assert(sizeof(WebRtcCallbackListReceivers) == 32);
static_assert(offsetof(WebRtcCallbackListReceivers, begin) == 0);
static_assert(offsetof(WebRtcCallbackListReceivers, end) == 8);
static_assert(offsetof(WebRtcCallbackListReceivers, capacity) == 16);
static_assert(offsetof(WebRtcCallbackListReceivers, sendInProgress) == 24);
static_assert(offsetof(GuardedReceivers, object) == 16);
static_assert(offsetof(GuardedReceivers, after) == 48);

}

int main() {
    std::array<GuardedReceivers, 32> objects;
    for (std::size_t index = 0; index < objects.size(); ++index) {
        auto& storage = objects[index];
        std::memset(&storage, static_cast<int>(0x80 + index), sizeof(storage));
    }
    for (std::size_t index = 0; index < objects.size(); ++index) {
        auto& storage = objects[index];
        _ZN6webrtc18callback_list_impl21CallbackListReceiversC1Ev_nid_postfix(&storage.object);
        Require(storage.object.begin == nullptr);
        Require(storage.object.end == nullptr);
        Require(storage.object.capacity == nullptr);
        Require(!storage.object.sendInProgress);
        for (auto byte : storage.before) Require(byte == 0x80 + index);
        for (auto byte : storage.after) Require(byte == 0x80 + index);
        for (std::size_t previous = 0; previous < index; ++previous) {
            Require(objects[previous].object.begin == nullptr);
            Require(objects[previous].object.end == nullptr);
            Require(objects[previous].object.capacity == nullptr);
            Require(!objects[previous].object.sendInProgress);
        }
    }
    try {
        _ZN6webrtc18callback_list_impl21CallbackListReceiversC1Ev_nid_postfix(nullptr);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    return 0;
}

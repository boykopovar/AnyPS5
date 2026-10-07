#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>

extern "C" {
void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* parameters);
int APS5_VABI sceAudio3dInitialize(std::int64_t reserved);
int APS5_VABI sceAudio3dTerminate();
int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id);
int APS5_VABI sceAudio3dPortClose(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortSetAttribute(std::uint32_t port_id, std::uint32_t attribute_id, const void* attribute, std::size_t attribute_size);
int APS5_VABI sceAudio3dPortGetQueueLevel(std::uint32_t port_id, std::uint32_t* queue_level, std::uint32_t* queue_available);
int APS5_VABI sceAudio3dPortAdvance(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortPush(std::uint32_t port_id, std::uint32_t blocking);
int APS5_VABI sceAudio3dObjectReserve(std::uint32_t port_id, std::uint32_t* object_id);
int APS5_VABI sceAudio3dObjectUnreserve(std::uint32_t port_id, std::uint32_t object_id);
}

namespace {

constexpr int INVALID_PORT = static_cast<int>(0x80EA0002);
constexpr int INVALID_OBJECT = static_cast<int>(0x80EA0003);
constexpr int INVALID_PARAMETER = static_cast<int>(0x80EA0004);
constexpr int OUT_OF_RESOURCES = static_cast<int>(0x80EA0006);
constexpr int NOT_READY = static_cast<int>(0x80EA0007);
constexpr int SYSTEM_USER = 0xFF;
constexpr std::uint32_t OBJECT_INVALID = 0xFFFFFFFF;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename F>
void RequireThrows(F f, const char* message) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, message);
}

Audio3dOpenParameters Defaults() {
    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    return parameters;
}

int Open(const Audio3dOpenParameters& parameters, std::uint32_t* id) {
    return sceAudio3dPortOpen(SYSTEM_USER, &parameters, id);
}

void RequireLevel(std::uint32_t level, std::uint32_t available, const char* message) {
    std::uint32_t got_level = 99;
    std::uint32_t got_available = 99;
    Require(sceAudio3dPortGetQueueLevel(0, &got_level, &got_available) == 0, message);
    Require(got_level == level && got_available == available, message);
}

void CheckBeforeInitialize() {
    std::uint32_t id = 7;
    Audio3dOpenParameters parameters = Defaults();
    Require(sceAudio3dTerminate() == NOT_READY, "terminate before initialize must be NOT_READY");
    Require(sceAudio3dInitialize(1) == INVALID_PARAMETER, "non-zero reserved must be rejected");
    Require(Open(parameters, &id) == NOT_READY, "open before initialize must be NOT_READY");
    Require(id == 7, "a failed open must not write the port id");
    Require(sceAudio3dPortOpen(1, nullptr, nullptr) == NOT_READY, "NOT_READY is checked before the arguments");
    std::uint32_t level = 0;
    Require(sceAudio3dPortGetQueueLevel(0, &level, nullptr) == INVALID_PORT, "queue level needs an open port");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "close needs an open port");
    Require(sceAudio3dPortAdvance(0) == INVALID_PORT, "advance needs an open port");
    Require(sceAudio3dPortPush(0, 0) == INVALID_PORT, "push needs an open port");
    float value = 0.0f;
    Require(sceAudio3dPortSetAttribute(0, 0x10001, &value, sizeof(value)) == INVALID_PORT, "set attribute needs an open port");
    Require(sceAudio3dInitialize(0) == 0, "initialize");
    RequireThrows([] { sceAudio3dInitialize(0); }, "repeated initialize must throw");
}

void CheckOpenParameters() {
    std::uint32_t id = 7;
    const Audio3dOpenParameters defaults = Defaults();
    Require(sceAudio3dPortOpen(1, &defaults, &id) == INVALID_PARAMETER, "user id must be the system user");
    Require(sceAudio3dPortOpen(SYSTEM_USER, nullptr, &id) == INVALID_PARAMETER, "null parameters");
    Require(sceAudio3dPortOpen(SYSTEM_USER, &defaults, nullptr) == INVALID_PARAMETER, "null port id");
    auto rejects = [&](auto change, const char* message) {
        Audio3dOpenParameters parameters = defaults;
        change(parameters);
        Require(Open(parameters, &id) == INVALID_PARAMETER, message);
        Require(id == 7, "a failed open must not write the port id");
    };
    rejects([](auto& p) { p.size_this = 0x30; }, "unknown parameter size");
    rejects([](auto& p) { p.size_this = 0x21; }, "parameter size is not masked");
    rejects([](auto& p) { p.rate = 1; }, "only 48 kHz");
    rejects([](auto& p) { p.granularity = 0x80; }, "granularity below 256");
    rejects([](auto& p) { p.granularity = 0x180; }, "granularity not a multiple of 256");
    rejects([](auto& p) { p.max_objects = 0; }, "zero objects");
    rejects([](auto& p) { p.queue_depth = 0; }, "zero queue depth");
    rejects([](auto& p) { p.buffer_mode = 3; }, "buffer mode above 2");
    rejects([](auto& p) { p.size_this = 0x28; p.num_beds = 4; }, "4 beds");
    rejects([](auto& p) { p.size_this = 0x28; p.num_beds = 1; }, "1 bed");
    auto throws = [&](auto change, const char* message) {
        Audio3dOpenParameters parameters = defaults;
        change(parameters);
        RequireThrows([&] { Open(parameters, &id); }, message);
    };
    throws([](auto& p) { p.size_this = 0x10; }, "0x10 parameters select buffer mode 0");
    throws([](auto& p) { p.size_this = 0x18; }, "0x18 parameters select buffer mode 1");
    throws([](auto& p) { p.buffer_mode = 1; }, "buffer mode 1");
    throws([](auto& p) { p.size_this = 0x28; p.num_beds = 3; }, "3 beds");
}

void CheckOpenClose() {
    std::uint32_t id = 7;
    Audio3dOpenParameters parameters = Defaults();
    parameters.size_this = 0x28;
    parameters.num_beds = 2;
    Require(Open(parameters, &id) == 0 && id == 0, "open hands out port 0");
    std::uint32_t second = 7;
    Require(Open(parameters, &second) == OUT_OF_RESOURCES && second == 7, "only one port");
    std::uint32_t level = 0;
    Require(sceAudio3dPortGetQueueLevel(1, &level, nullptr) == INVALID_PORT, "port 1 does not exist");
    Require(sceAudio3dPortClose(1) == INVALID_PORT, "close of port 1");
    Require(sceAudio3dPortClose(0) == 0, "close");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "double close");
    Require(sceAudio3dPortGetQueueLevel(0, &level, nullptr) == INVALID_PORT, "closed port");
}

void CheckAttributes() {
    float value = 0.5f;
    int flag = 1;
    Require(sceAudio3dPortSetAttribute(0, 0x10001, nullptr, 4) == INVALID_PARAMETER, "null attribute");
    Require(sceAudio3dPortSetAttribute(1, 0x10001, &value, 4) == INVALID_PORT, "attribute on port 1");
    Require(sceAudio3dPortSetAttribute(0, 0x10001, &value, 4) == 0, "late reverb level");
    Require(sceAudio3dPortSetAttribute(0, 0x10002, &value, 4) == 0, "downmix spread radius");
    Require(sceAudio3dPortSetAttribute(0, 0x10003, &flag, 4) == 0, "downmix spread height aware");
    RequireThrows([&] { sceAudio3dPortSetAttribute(0, 0x10004, &value, 4); }, "unknown attribute must throw");
    RequireThrows([&] { sceAudio3dPortSetAttribute(0, 0x10001, &value, 8); }, "wrong attribute size must throw");
}

void CheckObjects() {
    std::uint32_t object = 7;
    Require(sceAudio3dObjectReserve(0, nullptr) == INVALID_PARAMETER, "null object id");
    Require(sceAudio3dObjectReserve(0, &object) == INVALID_PORT && object == OBJECT_INVALID, "reserve needs an open port");
    Require(sceAudio3dObjectUnreserve(0, 1) == INVALID_PORT, "unreserve needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    parameters.max_objects = 2;
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    object = 7;
    Require(sceAudio3dObjectReserve(1, &object) == INVALID_PORT && object == OBJECT_INVALID, "reserve on port 1");
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 1, "first object is 1");
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 2, "second object is 2");
    Require(sceAudio3dObjectReserve(0, &object) == OUT_OF_RESOURCES && object == OBJECT_INVALID, "max_objects is the limit");
    Require(sceAudio3dObjectUnreserve(1, 1) == INVALID_PORT, "unreserve on port 1");
    Require(sceAudio3dObjectUnreserve(0, 3) == INVALID_OBJECT, "unreserve of an unknown object");
    Require(sceAudio3dObjectUnreserve(0, OBJECT_INVALID) == INVALID_OBJECT, "unreserve of the invalid object");
    Require(sceAudio3dObjectUnreserve(0, 1) == 0, "unreserve");
    Require(sceAudio3dObjectUnreserve(0, 1) == INVALID_OBJECT, "double unreserve");
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 3, "ids are not reused while the port is open");
    Require(sceAudio3dObjectReserve(0, &object) == OUT_OF_RESOURCES, "full again");
    Require(sceAudio3dPortClose(0) == 0, "close");
    Require(Open(parameters, &id) == 0 && id == 0, "reopen");
    Require(sceAudio3dObjectUnreserve(0, 2) == INVALID_OBJECT, "close drops the objects");
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 1, "ids restart on a new port");
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 2, "a new port has the full limit");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckQueue() {
    using namespace std::chrono;
    constexpr std::uint32_t granularity = 0x1800;
    constexpr auto frame = microseconds(1000000ull * granularity / 48000);
    Audio3dOpenParameters parameters = Defaults();
    parameters.granularity = granularity;
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    CheckAttributes();
    Require(sceAudio3dPortGetQueueLevel(0, nullptr, nullptr) == INVALID_PARAMETER, "both outputs null");
    std::uint32_t available = 0;
    Require(sceAudio3dPortGetQueueLevel(0, nullptr, &available) == 0 && available == 2, "level pointer is optional");
    RequireLevel(0, 2, "empty queue");
    Require(sceAudio3dPortPush(0, 1) == 0, "push of an empty queue");
    RequireLevel(0, 2, "empty push queues nothing");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 1");
    RequireLevel(1, 1, "one frame queued");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 2");
    RequireLevel(2, 0, "queue full");
    RequireThrows([] { sceAudio3dPortAdvance(0); }, "advance into a full queue must throw");
    RequireThrows([] { sceAudio3dPortPush(0, 2); }, "unknown blocking mode must throw");

    auto start = steady_clock::now();
    Require(sceAudio3dPortPush(0, 1) == 0, "blocking push");
    auto waited = steady_clock::now() - start;
    Require(waited >= frame - milliseconds(2), "blocking push waits until one frame has played");
    RequireLevel(1, 1, "blocking push returns with one free slot");

    Require(sceAudio3dPortAdvance(0) == 0, "advance 3");
    start = steady_clock::now();
    Require(sceAudio3dPortPush(0, 0) == 0, "async push");
    Require(steady_clock::now() - start < frame, "async push does not wait for a frame");
    std::uint32_t level = 0;
    Require(sceAudio3dPortGetQueueLevel(0, &level, nullptr) == 0 && level >= 1, "async push keeps frames queued");
    std::this_thread::sleep_for(frame * 3);
    RequireLevel(0, 2, "frames drain at the port's rate");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckTerminate() {
    std::uint32_t id = 7;
    const Audio3dOpenParameters parameters = Defaults();
    Require(Open(parameters, &id) == 0 && id == 0, "open before terminate");
    Require(sceAudio3dTerminate() == NOT_READY, "terminate with an open port must be NOT_READY");
    Require(sceAudio3dPortClose(0) == 0, "close before terminate");
    Require(sceAudio3dTerminate() == 0, "terminate");
    Require(sceAudio3dTerminate() == NOT_READY, "double terminate must be NOT_READY");
    id = 7;
    Require(Open(parameters, &id) == NOT_READY && id == 7, "open after terminate must be NOT_READY");
    Require(sceAudio3dInitialize(0) == 0, "initialize after terminate");
    Require(Open(parameters, &id) == 0 && id == 0, "open after reinitialize");
    Require(sceAudio3dPortClose(0) == 0, "close after reinitialize");
    Require(sceAudio3dTerminate() == 0, "terminate after reinitialize");
}

}

int main() {
    CheckBeforeInitialize();
    CheckOpenParameters();
    CheckOpenClose();
    CheckObjects();
    CheckQueue();
    CheckTerminate();
}

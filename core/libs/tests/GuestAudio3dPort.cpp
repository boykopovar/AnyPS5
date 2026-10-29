#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <optional>
#include <thread>
#include <vector>

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

template <typename F>
std::optional<int> OpenOutcome(F open, std::uint32_t& id, const char* message) {
    id = 7;
    std::optional<int> result;
    try {
        result = open();
    } catch (const std::exception&) {
    }
    if (result == 0) {
        Require(id == 0, message);
        Require(sceAudio3dPortClose(0) == 0, message);
        return result;
    }
    std::uint32_t level = 0;
    Require(id == 7, message);
    Require(sceAudio3dPortGetQueueLevel(0, &level, nullptr) == INVALID_PORT, message);
    return result;
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
    std::uint32_t num_attributes = 3;
    std::uint32_t attributes[3]{};
    Require(sceAudio3dPortGetAttributesSupported(0, attributes, &num_attributes) == INVALID_PORT, "supported attributes need an open port");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "close needs an open port");
    Require(sceAudio3dPortAdvance(0) == INVALID_PORT, "advance needs an open port");
    Require(sceAudio3dPortPush(0, 0) == INVALID_PORT, "push needs an open port");
    float value = 0.0f;
    Require(sceAudio3dPortSetAttribute(0, 0x10001, &value, sizeof(value)) == INVALID_PORT, "set attribute needs an open port");
    Require(sceAudio3dPortCreate(0x100, 0, 0, &id) == NOT_READY && id == 7, "create before initialize must be NOT_READY");
    Require(sceAudio3dPortCreate(0x100, 0, 1, &id) == INVALID_PARAMETER && id == 7, "create checks reserved before the initialization");
    Require(sceAudio3dPortCreate(0x100, 0, 0, nullptr) == INVALID_PARAMETER, "create checks the port id before the initialization");
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
    rejects([](auto& p) { p.size_this = 0x10; p.rate = 1; }, "0x10 parameters check the rate");
    rejects([](auto& p) { p.size_this = 0x10; p.granularity = 0x180; }, "0x10 parameters check the granularity");
    rejects([](auto& p) { p.size_this = 0x18; p.max_objects = 0; }, "0x18 parameters check the object count");
    rejects([](auto& p) { p.size_this = 0x18; p.queue_depth = 0; }, "0x18 parameters check the queue depth");
    auto outcome = [&](auto change, const char* message) {
        Audio3dOpenParameters parameters = defaults;
        change(parameters);
        return OpenOutcome([&] { return Open(parameters, &id); }, id, message);
    };
    const std::optional<int> legacy = outcome([](auto& p) { p.size_this = 0x10; }, "a 0x10 block opens port 0 or leaves no port");
    const std::optional<int> legacy_tail = outcome([](auto& p) { p.size_this = 0x10; p.max_objects = 0; p.queue_depth = 0; p.buffer_mode = 3; }, "a 0x10 block opens port 0 or leaves no port");
    Require(legacy_tail == legacy, "0x10 parameters end at the rate");
    const std::optional<int> short_block = outcome([](auto& p) { p.size_this = 0x18; }, "a 0x18 block opens port 0 or leaves no port");
    const std::optional<int> short_tail = outcome([](auto& p) { p.size_this = 0x18; p.buffer_mode = 3; p.num_beds = 4; }, "a 0x18 block opens port 0 or leaves no port");
    Require(short_tail == short_block, "0x18 parameters end at the queue depth");
    outcome([](auto& p) { p.buffer_mode = 1; }, "buffer mode 1 opens port 0 or leaves no port");
    outcome([](auto& p) { p.size_this = 0x28; p.num_beds = 3; }, "3 beds open port 0 or leave no port");
}

void CheckPortCreate() {
    std::uint32_t id = 7;
    Require(sceAudio3dPortCreate(0x100, 0, 0, nullptr) == INVALID_PARAMETER, "create needs a port id");
    Require(sceAudio3dPortCreate(0x100, 0, 1, &id) == INVALID_PARAMETER && id == 7, "create needs a zero reserved argument");
    Require(sceAudio3dPortCreate(0x100, 0, -1, &id) == INVALID_PARAMETER && id == 7, "create rejects a negative reserved argument");
    Require(sceAudio3dPortCreate(0x100, 1, 0, &id) == INVALID_PARAMETER && id == 7, "create only at 48 kHz");
    Require(sceAudio3dPortCreate(0x80, 0, 0, &id) == INVALID_PARAMETER && id == 7, "create granularity below 256");
    Require(sceAudio3dPortCreate(0x180, 0, 0, &id) == INVALID_PARAMETER && id == 7, "create granularity not a multiple of 256");
    Audio3dOpenParameters block = Defaults();
    block.size_this = 0x10;
    for (const std::uint32_t granularity : {0x100u, 0x400u}) {
        block.granularity = granularity;
        const std::optional<int> created = OpenOutcome([&] { return sceAudio3dPortCreate(granularity, 0, 0, &id); }, id, "create opens port 0 or leaves no port");
        const std::optional<int> opened = OpenOutcome([&] { return Open(block, &id); }, id, "a 0x10 block opens port 0 or leaves no port");
        Require(created == opened, "create opens like a 0x10 block with its granularity");
    }
    Require(Open(Defaults(), &id) == 0 && id == 0, "open after create");
    std::uint32_t second = 7;
    Require(sceAudio3dPortCreate(0x100, 1, 0, &second) == INVALID_PARAMETER && second == 7, "create checks its arguments while a port is open");
    std::optional<int> again;
    try {
        again = sceAudio3dPortCreate(0x100, 0, 0, &second);
    } catch (const std::exception&) {
    }
    Require(again != 0 && second == 7, "create opens no second port");
    RequireLevel(0, 2, "create leaves the open port as it was");
    Require(sceAudio3dPortClose(0) == 0, "close the port from open");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "create opened no second port");
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
    constexpr std::uint32_t supported[]{0x10001, 0x10002, 0x10003};
    std::uint32_t num_attributes = 99;
    Require(sceAudio3dPortGetAttributesSupported(0, nullptr, &num_attributes) == 0 && num_attributes == 3, "null output reports the supported attribute count");
    Require(sceAudio3dPortGetAttributesSupported(1, nullptr, &num_attributes) == INVALID_PORT, "supported attributes reject an invalid port");
    Require(sceAudio3dPortGetAttributesSupported(0, nullptr, nullptr) == INVALID_PARAMETER, "supported attributes require a count pointer");
    Require(sceAudio3dPortGetAttributesSupported(1, nullptr, nullptr) == INVALID_PORT, "port validation precedes count validation");
    num_attributes = 0;
    std::uint32_t output[4]{0, 0, 0, 99};
    Require(sceAudio3dPortGetAttributesSupported(0, output, &num_attributes) == 0 && num_attributes == 3, "the count is output only");
    Require(output[0] == supported[0] && output[1] == supported[1] && output[2] == supported[2] && output[3] == 99, "the complete list is written without touching the following element");
    num_attributes = 3;
    std::uint32_t all[3]{};
    Require(sceAudio3dPortGetAttributesSupported(0, all, &num_attributes) == 0 && num_attributes == 3, "all supported attributes are returned");
    Require(std::equal(std::begin(all), std::end(all), std::begin(supported)), "the reported attributes match the port setter");
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
    Audio3dOpenParameters parameters = Defaults();
    parameters.granularity = 0x1800;
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    CheckAttributes();
    Require(sceAudio3dPortGetQueueLevel(0, nullptr, nullptr) == INVALID_PARAMETER, "both outputs null");
    std::uint32_t available = 0;
    Require(sceAudio3dPortGetQueueLevel(0, nullptr, &available) == 0 && available == 2, "level pointer is optional");
    std::uint32_t level = 99;
    Require(sceAudio3dPortGetQueueLevel(0, &level, nullptr) == 0 && level == 0, "available pointer is optional");
    RequireLevel(0, 2, "empty queue");
    Require(sceAudio3dPortPush(0, 1) == 0, "push of an empty queue");
    RequireLevel(0, 2, "empty push queues nothing");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 1");
    RequireLevel(1, 1, "one frame queued");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 2");
    RequireLevel(2, 0, "queue full");
    Require(sceAudio3dPortAdvance(0) == NOT_READY, "advance into a full queue must be NOT_READY");
    RequireThrows([] { sceAudio3dPortPush(0, 2); }, "unknown blocking mode must throw");
    Require(sceAudio3dPortPush(0, 1) == 0, "sync push");
    RequireLevel(0, 2, "sync push drains every queued frame");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 3");
    Require(sceAudio3dPortAdvance(0) == 0, "advance 4");
    Require(sceAudio3dPortPush(0, 0) == 0, "async push");
    RequireLevel(1, 1, "async push keeps the queue one below the depth");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckMixAndPush() {
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    Require(sceAudio3dPortAdvance(0) == 0, "advance mixes an empty bed into silence");
    RequireLevel(1, 1, "the silent frame is queued");
    const std::int16_t samples[8] = {1000, -1000, 2000, -2000, 3000, -3000, 4000, -4000};
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "bed write");
    Require(sceAudio3dPortAdvance(0) == 0, "advance mixes the bed");
    RequireLevel(2, 0, "the mixed frame is queued");
    Require(sceAudio3dPortAdvance(0) == NOT_READY, "a full queue rejects the mix");
    Require(sceAudio3dPortPush(0, 1) == 0, "sync push");
    RequireLevel(0, 2, "sync push drains every queued frame");
    Require(sceAudio3dPortAdvance(0) == 0, "advance after the sync drain");
    Require(sceAudio3dPortAdvance(0) == 0, "advance fills the queue again");
    Require(sceAudio3dPortPush(0, 0) == 0, "async push");
    RequireLevel(1, 1, "async push leaves the queue one below the depth");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckPushKeepsRealTime() {
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    const std::int16_t samples[8] = {1000, -1000, 2000, -2000, 3000, -3000, 4000, -4000};
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "bed write");
    constexpr std::uint32_t frames = 200;
    const std::uint64_t blockUs = 1000000ull * parameters.granularity / 48000ull;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i < frames; i++) {
        Require(sceAudio3dPortAdvance(0) == 0, "advance");
        Require(sceAudio3dPortPush(0, 1) == 0, "sync push");
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    const std::uint64_t expected = frames * blockUs;
    Require(elapsed + 150000 >= static_cast<std::uint64_t>(expected), "a blocking push must run at real time");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckFlush() {
    Require(sceAudio3dPortFlush(0) == INVALID_PORT, "flush needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    Require(sceAudio3dPortAdvance(0) == 0, "advance");
    RequireLevel(1, 1, "one frame queued");
    Require(sceAudio3dPortFlush(1) == INVALID_PORT, "flush of port 1");
    Require(sceAudio3dPortFlush(0) == 0, "flush");
    RequireLevel(0, 2, "flush drops the queued frames");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckAudioOutPorts() {
    const std::uint32_t granularity = Defaults().granularity;
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity, 48000, 1) == INVALID_PORT, "audio out open needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    Require(sceAudio3dAudioOutOpen(1, SYSTEM_USER, 126, 0, granularity, 48000, 1) == INVALID_PORT, "audio out open on port 1");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity + 0x100, 48000, 1) == INVALID_PARAMETER, "len must equal the granularity");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 1, granularity, 48000, 1) == INVALID_PARAMETER, "index must be 0");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity, 48000, 8) == INVALID_PARAMETER, "unknown format");
    const int handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity, 48000, 1);
    Require(handle > 0, "audio out open returns a handle");
    const std::vector<std::int16_t> block(static_cast<std::size_t>(granularity) * 2, 256);
    for (std::uint32_t i = 0; i < parameters.queue_depth; ++i) {
        Require(sceAudio3dAudioOutOutput(handle, block.data()) == static_cast<int>(granularity), "audio out output returns the sample count");
    }
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == NOT_READY, "a full pending queue must be NOT_READY");
    Require(sceAudio3dPortPush(0, 1) == 0, "sync push drains the associated ports");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == static_cast<int>(granularity), "push drains the pending queue");
    Require(sceAudio3dPortFlush(0) == 0, "flush drains the associated ports");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == static_cast<int>(granularity), "flush resets the pending queue");
    Require(sceAudio3dAudioOutOutputs(nullptr, 0) == INVALID_PARAMETER, "outputs rejects an empty batch");
    Require(sceAudio3dAudioOutClose(handle) == 0, "audio out close");
    Require(sceAudio3dAudioOutClose(handle) == INVALID_PORT, "audio out double close");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == INVALID_PORT, "output on a closed handle");
    const int open_handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity, 48000, 1);
    Require(open_handle > 0, "audio out reopen");
    Require(sceAudio3dPortClose(0) == 0, "close");
    Require(sceAudio3dAudioOutOutput(open_handle, block.data()) == INVALID_PORT, "port close clears the associated ports");
    Require(sceAudio3dAudioOutClose(open_handle) == INVALID_PORT, "port close closes the associated ports");
}

void CheckObjectAttributes() {
    float value = 0.5f;
    Audio3dAttribute attribute{0x10001, 0, &value, sizeof(value)};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &attribute) == INVALID_PORT, "attributes need an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    std::uint32_t object = 0;
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 1, "reserve");
    Require(sceAudio3dObjectSetAttributes(1, 1, 1, &attribute) == INVALID_PORT, "attributes on port 1");
    Require(sceAudio3dObjectSetAttributes(0, 1, 0, &attribute) == INVALID_PARAMETER, "zero attributes");
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, nullptr) == INVALID_PARAMETER, "null array");
    Require(sceAudio3dObjectSetAttributes(0, 2, 1, &attribute) == INVALID_OBJECT, "unreserved object");
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &attribute) == 0, "set one attribute");
    Audio3dAttribute reset{0x20000, 0, nullptr, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &reset) == 0, "reset state takes a null value");
    Audio3dAttribute missing{0x10001, 0, nullptr, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &missing) == INVALID_PARAMETER, "a value is required");
    Require(sceAudio3dObjectUnreserve(0, 1) == 0, "unreserve");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckBed() {
    const std::int16_t samples[8] = {};
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == INVALID_PORT, "bed write needs an open port");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 0, false) == INVALID_PORT, "bed write2 needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    const auto consume = [] {
        Require(sceAudio3dPortAdvance(0) == 0, "advance after a bed write");
        Require(sceAudio3dPortFlush(0) == 0, "flush the mixed frame after a bed write");
    };
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "2 channel s16 bed write");
    consume();
    Require(sceAudio3dBedWrite(0, 1, 0, samples, 4) == INVALID_PARAMETER, "1 channel bed write");
    Require(sceAudio3dBedWrite(0, 2, 0, nullptr, 4) == INVALID_PARAMETER, "null bed buffer");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 0, false) == 0, "bed write2 route both");
    consume();
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 1, false) == 0, "bed write2 route hmu only");
    consume();
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 2, false) == 0, "bed write2 route tv only");
    consume();
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 3, false) == INVALID_PARAMETER, "bed write2 unknown route");
    Require(sceAudio3dBedWrite(0, 2, 2, samples, 4) == INVALID_PARAMETER, "unknown bed format");
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 0) == INVALID_PARAMETER, "zero bed samples");
    Require(sceAudio3dBedWrite(0, 2, 0, reinterpret_cast<const char*>(samples) + 1, 4) == INVALID_PARAMETER, "misaligned s16 bed buffer");
    const float floats[8] = {};
    Require(sceAudio3dBedWrite(0, 2, 1, floats, 4) == 0, "float bed write");
    consume();
    const std::uint32_t granularity = parameters.granularity;
    std::vector<std::int16_t> wide(static_cast<std::size_t>(granularity) * 8, 0);
    Require(sceAudio3dBedWrite(0, 6, 0, wide.data(), granularity) == 0, "6 channel bed write");
    consume();
    Require(sceAudio3dBedWrite(0, 8, 0, wide.data(), granularity) == 0, "8 channel bed write");
    consume();
    Require(sceAudio3dPortClose(0) == 0, "close");
}

void CheckBedQueue() {
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    const std::int16_t samples[8] = {1000, -1000, 2000, -2000, 3000, -3000, 4000, -4000};
    for (std::uint32_t i = 0; i < parameters.queue_depth; ++i) {
        Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "each bed write fills the queue");
    }
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == NOT_READY, "a full bed queue must be NOT_READY");
    Require(sceAudio3dPortAdvance(0) == 0, "advance consumes one bed");
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "the consumed bed makes room");
    Require(sceAudio3dPortAdvance(0) == 0, "advance consumes the next bed");
    RequireLevel(2, 0, "two frames are queued from the queued beds");
    Require(sceAudio3dPortFlush(0) == 0, "flush");
    Require(sceAudio3dPortAdvance(0) == 0, "advance with no bed left mixes silence");
    RequireLevel(1, 1, "silence is queued behind the flushed frames");
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
    CheckPortCreate();
    CheckObjects();
    CheckQueue();
    CheckMixAndPush();
    CheckPushKeepsRealTime();
    CheckFlush();
    CheckAudioOutPorts();
    CheckObjectAttributes();
    CheckBed();
    CheckBedQueue();
    CheckTerminate();
}

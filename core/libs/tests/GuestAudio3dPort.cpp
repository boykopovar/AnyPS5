#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr int INVALID_PORT = static_cast<int>(0x80EA0002);
constexpr int INVALID_OBJECT = static_cast<int>(0x80EA0003);
constexpr int INVALID_PARAMETER = static_cast<int>(0x80EA0004);
constexpr int OUT_OF_MEMORY = static_cast<int>(0x80EA0005);
constexpr int OUT_OF_RESOURCES = static_cast<int>(0x80EA0006);
constexpr int NOT_READY = static_cast<int>(0x80EA0007);
constexpr int AUDIO_OUT_INVALID_FORMAT = static_cast<int>(0x80260007);
constexpr int AUDIO_OUT_INVALID_PORT_TYPE = static_cast<int>(0x8026000A);
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
    std::uint32_t num_attributes = 3;
    std::uint32_t attributes[3]{};
    Require(sceAudio3dPortGetAttributesSupported(0, attributes, &num_attributes) == INVALID_PORT, "supported attributes need an open port");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "close needs an open port");
    Require(sceAudio3dPortDestroy(0) == INVALID_PORT, "destroy needs an open port");
    std::uint32_t status = 7;
    Require(sceAudio3dPortGetStatus(0, &status, nullptr, 0) == INVALID_PORT && status == 7, "status needs an open port");
    Require(sceAudio3dPortOpenEx(SYSTEM_USER, &parameters, &id) == NOT_READY && id == 7, "open ex before initialize must be NOT_READY");
    Require(sceAudio3dPortCreate(0x100, 0, 0, &id) == NOT_READY && id == 7, "create before initialize must be NOT_READY");
    Require(sceAudio3dPortCreate(0x100, 0, 1, &id) == INVALID_PARAMETER && id == 7, "create checks the reserved argument first");
    Require(sceAudio3dPortCreate(0x100, 0, 0, nullptr) == NOT_READY, "create checks the initialization before the port id");
    auto* const inaccessible = reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10});
    Require(Open(parameters, inaccessible) == NOT_READY, "a failed open without report handlers does not read the port id");
    Require(sceAudio3dPortCreate(0x100, 0, 1, inaccessible) == INVALID_PARAMETER, "a failed create without report handlers does not read the port id");
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
    rejects([](auto& p) { p.buffer_mode = 0; }, "a PS5 process cannot use buffer mode 0");
    rejects([](auto& p) { p.buffer_mode = 1; }, "a PS5 process cannot use buffer mode 1");
    rejects([](auto& p) { p.size_this = 0x10; }, "0x10 parameters select buffer mode 0");
    rejects([](auto& p) { p.size_this = 0x18; }, "0x18 parameters select buffer mode 1");
    for (const std::uint32_t beds : {0u, 1u, 3u, 4u, 0xFFFFFFFFu}) {
        Audio3dOpenParameters parameters = defaults;
        parameters.size_this = 0x28;
        parameters.num_beds = beds;
        id = 7;
        Require(Open(parameters, &id) == 0 && id == 0, "a PS5 process ignores the bed count");
        Require(sceAudio3dPortClose(0) == 0, "close after any bed count");
    }
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

void CheckPortWrappers() {
    Require(std::strcmp(sceAudio3dStrError(INVALID_PORT), "port does not exist") == 0, "invalid port text");
    Require(std::strcmp(sceAudio3dStrError(INVALID_OBJECT), "object does not exist") == 0, "invalid object text");
    Require(std::strcmp(sceAudio3dStrError(INVALID_PARAMETER), "invalid parameter") == 0, "invalid parameter text");
    Require(std::strcmp(sceAudio3dStrError(OUT_OF_MEMORY), "out of memory") == 0, "out of memory text");
    Require(std::strcmp(sceAudio3dStrError(OUT_OF_RESOURCES), "out of resources") == 0, "out of resources text");
    Require(std::strcmp(sceAudio3dStrError(NOT_READY), "not ready") == 0, "not ready text");
    for (const int error : {0, -1, static_cast<int>(0x80EA0001), static_cast<int>(0x80EA0008), static_cast<int>(0x80260002)}) {
        Require(std::strcmp(sceAudio3dStrError(error), "(unknown)") == 0, "other codes are unknown");
    }
    std::uint32_t id = 7;
    std::uint32_t status = 7;
    Require(sceAudio3dPortCreate(0x100, 0, 1, &id) == INVALID_PARAMETER && id == 7, "create needs a zero reserved argument");
    Require(sceAudio3dPortCreate(0x100, 0, 0, &id) == INVALID_PARAMETER && id == 7, "create selects buffer mode 0");
    Require(sceAudio3dPortCreate(0x100, 0, 0, nullptr) == INVALID_PARAMETER, "create without a port id");
    const Audio3dOpenParameters parameters = Defaults();
    Require(sceAudio3dPortOpenEx(1, &parameters, &id) == INVALID_PARAMETER && id == 7, "open ex checks like open");
    Require(sceAudio3dPortOpenEx(SYSTEM_USER, &parameters, &id) == 0 && id == 0, "open ex hands out port 0");
    id = 7;
    Require(Open(parameters, &id) == OUT_OF_RESOURCES && id == 7, "open ex takes the only port");
    Require(sceAudio3dPortCreate(0x100, 0, 0, &id) == INVALID_PARAMETER && id == 7, "create is checked before the port count");
    Require(sceAudio3dPortGetStatus(1, &status, nullptr, 0) == INVALID_PORT && status == 7, "status of port 1");
    Require(sceAudio3dPortGetStatus(0, nullptr, nullptr, 0) == 0, "status without outputs");
    char description[32];
    std::memset(description, 'x', sizeof(description));
    Require(sceAudio3dPortGetStatus(0, &status, description, sizeof(description)) == 0, "status");
    Require(status == 1 && std::strcmp(description, "Downmix ()") == 0, "the port reports a downmix");
    std::memset(description, 'x', sizeof(description));
    Require(sceAudio3dPortGetStatus(0, nullptr, description, 4) == 0, "status into a short buffer");
    Require(std::strcmp(description, "Dow") == 0 && description[4] == 'x', "the description is truncated");
    std::memset(description, 'x', sizeof(description));
    Require(sceAudio3dPortGetStatus(0, nullptr, description, 0) == 0 && description[0] == 'x', "a zero size writes nothing");
    std::memset(description, 'x', sizeof(description));
    Require(sceAudio3dPortGetStatus(0, nullptr, description, SIZE_MAX) == 0, "status with an unbounded size");
    Require(std::strcmp(description, "Downmix ()") == 0 && description[11] == 'x', "an unbounded size still writes the description");
    Require(sceAudio3dPortDestroy(1) == INVALID_PORT, "destroy of port 1");
    Require(sceAudio3dPortDestroy(0) == 0, "destroy");
    Require(sceAudio3dPortDestroy(0) == INVALID_PORT, "double destroy");
    Require(sceAudio3dPortClose(0) == INVALID_PORT, "destroy closes the port");
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
    Require(sceAudio3dPortSetAttribute(1, 0x10004, &value, 4) == INVALID_PORT, "the port is checked before the attribute id");
    Require(sceAudio3dPortSetAttribute(0, 0x10001, &value, 4) == 0, "late reverb level");
    Require(sceAudio3dPortSetAttribute(0, 0x10002, &value, 4) == 0, "downmix spread radius");
    Require(sceAudio3dPortSetAttribute(0, 0x10003, &flag, 4) == 0, "downmix spread height aware");
    Require(sceAudio3dPortSetAttribute(0, 0x10003, &flag, 8) == 0, "height aware ignores the size");
    Require(sceAudio3dPortSetAttribute(0, 0x10003, nullptr, 4) == INVALID_PARAMETER, "height aware needs a value");
    Require(sceAudio3dPortSetAttribute(0, 0x10004, &value, 4) == INVALID_PARAMETER, "unknown attribute");
    Require(sceAudio3dPortSetAttribute(0, 0x10001, &value, 8) == INVALID_PARAMETER, "a float attribute is 4 bytes");
    const auto set_float = [](std::uint32_t attribute_id, float level) {
        return sceAudio3dPortSetAttribute(0, attribute_id, &level, sizeof(level));
    };
    Require(set_float(0x10001, 0.0f) == 0 && set_float(0x10001, 1.0f) == 0, "late reverb level accepts 0 to 1");
    Require(set_float(0x10001, -0.25f) == INVALID_PARAMETER && set_float(0x10001, 1.5f) == INVALID_PARAMETER, "late reverb level outside 0 to 1");
    Require(set_float(0x10001, std::numeric_limits<float>::quiet_NaN()) == INVALID_PARAMETER, "late reverb level must be finite");
    Require(set_float(0x10002, 0.1f) == 0 && set_float(0x10002, 2.0f) == 0, "downmix spread radius accepts 0.1 to 2");
    Require(set_float(0x10002, 0.05f) == INVALID_PARAMETER && set_float(0x10002, 2.5f) == INVALID_PARAMETER, "downmix spread radius outside 0.1 to 2");
    Require(set_float(0x10002, std::numeric_limits<float>::infinity()) == INVALID_PARAMETER, "downmix spread radius must be finite");
}

void CheckObjects() {
    std::uint32_t object = 7;
    Require(sceAudio3dObjectReserve(0, nullptr) == INVALID_PORT, "the port is checked before the object pointer");
    Require(sceAudio3dObjectReserve(0, &object) == INVALID_PORT && object == 7, "reserve needs an open port and leaves the id");
    Require(sceAudio3dObjectUnreserve(0, 1) == INVALID_PORT, "unreserve needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    parameters.max_objects = 2;
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    Require(sceAudio3dObjectReserve(0, nullptr) == INVALID_PARAMETER, "null object id");
    Require(sceAudio3dObjectReserve(1, &object) == INVALID_PORT && object == 7, "reserve on port 1");
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
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 1) == INVALID_PORT, "audio out open needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    Require(sceAudio3dAudioOutOpen(1, SYSTEM_USER, 0, 0, granularity, 48000, 1) == INVALID_PORT, "audio out open on port 1");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity + 0x100, 48000, 1) == INVALID_PARAMETER, "len must equal the granularity");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, -1, 0, granularity, 48000, 1) == INVALID_PARAMETER, "a negative type");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 5, 0, granularity, 48000, 8) == AUDIO_OUT_INVALID_PORT_TYPE, "the type is checked before the format");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 126, 0, granularity, 48000, 1) == AUDIO_OUT_INVALID_PORT_TYPE, "the Audio3d port type is not open to callers");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 8) == AUDIO_OUT_INVALID_FORMAT, "unknown format");
    const int handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 1);
    Require(handle > 0, "audio out open returns a handle");
    const std::vector<std::int16_t> block(static_cast<std::size_t>(granularity) * 2, 256);
    Require(sceAudio3dAudioOutOutput(0x1234, nullptr) == INVALID_PARAMETER, "the block is checked before the handle");
    for (std::uint32_t i = 0; i < parameters.queue_depth; ++i) {
        Require(sceAudio3dAudioOutOutput(handle, block.data()) == 0, "audio out output");
    }
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == NOT_READY, "a full pending queue must be NOT_READY");
    Require(sceAudio3dPortPush(0, 1) == 0, "sync push drains the associated ports");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == 0, "push drains the pending queue");
    Require(sceAudio3dPortFlush(0) == 0, "flush drains the associated ports");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == 0, "flush resets the pending queue");
    Require(sceAudio3dAudioOutOutputs(nullptr, 0) == INVALID_PARAMETER, "outputs rejects an empty batch");
    Require(sceAudio3dPortFlush(0) == 0, "flush before outputs");
    AudioOutOutputParam batch[2] = {{handle, block.data()}, {0x1234, block.data()}};
    Require(sceAudio3dAudioOutOutputs(batch, 1) == 0, "outputs one block");
    Require(sceAudio3dAudioOutOutputs(batch, 2) == INVALID_PORT, "outputs stops at an unknown handle");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == NOT_READY, "outputs queued both blocks for the known handle");
    Require(sceAudio3dAudioOutClose(handle) == 0, "audio out close");
    Require(sceAudio3dAudioOutClose(handle) == INVALID_PORT, "audio out double close");
    Require(sceAudio3dAudioOutOutput(handle, block.data()) == INVALID_PORT, "output on a closed handle");
    const int open_handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 1);
    Require(open_handle > 0, "audio out reopen");
    Require(sceAudio3dPortClose(0) == 0, "close");
    Require(sceAudio3dAudioOutOutput(open_handle, block.data()) == INVALID_PORT, "port close clears the associated ports");
    Require(sceAudio3dAudioOutClose(open_handle) == 0, "port close leaves the audio out handle open");
    Require(sceAudio3dAudioOutClose(open_handle) == INVALID_PORT, "audio out double close after port close");
}

void CheckObjectAttributes() {
    float value = 0.5f;
    Audio3dAttribute attribute{5, 0, &value, sizeof(value)};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &attribute) == INVALID_PORT, "attributes need an open port");
    Require(sceAudio3dObjectSetAttributes(0, 1, 0, &attribute) == INVALID_PARAMETER, "the count is checked before the port");
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, nullptr) == INVALID_PARAMETER, "the array is checked before the port");
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
    Require(sceAudio3dObjectSetAttributes(0, 0x80000001, 1, &attribute) == 0, "the top bit of the object id is ignored");
    for (const std::uint32_t attribute_id : {0u, 12u, 0x10001u, 0x20000u}) {
        Audio3dAttribute unknown{attribute_id, 0, &value, sizeof(value)};
        Require(sceAudio3dObjectSetAttributes(0, 1, 1, &unknown) == INVALID_PARAMETER, "object attribute ids are 1 to 11");
        Require(sceAudio3dObjectSetAttributes(0, 2, 1, &unknown) == INVALID_PARAMETER, "the ids are checked before the object");
    }
    Audio3dAttribute reset{7, 0, nullptr, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &reset) == 0, "reset state takes a null value");
    Audio3dAttribute missing{5, 0, nullptr, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &missing) == INVALID_PARAMETER, "a value is required");
    Require(sceAudio3dObjectSetAttributes(0, 2, 1, &missing) == INVALID_OBJECT, "the object is checked before the values");
    const std::int16_t samples[4] = {};
    Audio3dPcm pcm{0, samples, 4};
    Audio3dAttribute block{1, 0, &pcm, sizeof(pcm)};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &block) == 0, "s16 pcm");
    pcm.format = 1;
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &block) == 0, "float pcm");
    pcm.format = 2;
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &block) == INVALID_PARAMETER, "unknown pcm format");
    pcm = Audio3dPcm{0, samples, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &block) == INVALID_PARAMETER, "pcm needs samples");
    Require(sceAudio3dObjectSetAttributes(0, 2, 1, &block) == INVALID_PARAMETER, "pcm is checked before the object");
    Audio3dAttribute no_pcm{1, 0, nullptr, 0};
    Require(sceAudio3dObjectSetAttributes(0, 1, 1, &no_pcm) == INVALID_PARAMETER, "pcm needs a block");
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
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 1, false) == INVALID_PARAMETER, "bed write2 route hmu only is a PS4 route");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 2, false) == INVALID_PARAMETER, "bed write2 route tv only is a PS4 route");
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

struct ReportCopy {
    Audio3dReport report;
    std::string function;
    std::string argument_types;
    std::vector<std::uint64_t> arguments;
    void* user_data;
};

std::vector<ReportCopy> g_reports;

void APS5_VABI RecordReport(const Audio3dReport* report, void* user_data) {
    ReportCopy copy{*report, report->function, report->argument_types, {}, user_data};
    std::size_t count = 0;
    for (const char* type = report->argument_types; *type != '\0'; ++type) {
        if (*type == '&') continue;
        count += *type == 'A' || *type == 'M' ? 2 : 1;
    }
    const auto* overflow = static_cast<const std::uint64_t*>(report->arguments.overflow_arg_area);
    copy.arguments.assign(overflow, overflow + count);
    g_reports.push_back(std::move(copy));
}

void RequireReport(const ReportCopy& copy, const char* function, const char* types, std::uint32_t port_id, std::uint32_t object_id, int result,
    const std::vector<std::uint64_t>& arguments, const char* message) {
    Require(copy.report.size == 0x40 && copy.report.type == 0, message);
    Require(copy.function == function && copy.argument_types == types, message);
    Require(copy.report.port_id == port_id && copy.report.object_id == object_id && copy.report.result == result, message);
    Require(copy.report.arguments.gp_offset == 0x30 && copy.report.arguments.fp_offset == 0x30 && copy.report.arguments.reg_save_area != nullptr, message);
    Require(copy.arguments == arguments, message);
}

std::uint64_t Address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

void CheckReportHandlers() {
    std::uint64_t handle = 7;
    Require(sceAudio3dReportRegisterHandler(4, RecordReport, nullptr, &handle) == -4 && handle == 7, "report type 4 is out of range");
    Require(sceAudio3dReportRegisterHandler(0, nullptr, nullptr, &handle) == -2 && handle == 7, "a report handler is required");
    std::uint64_t handles[8];
    for (std::uint64_t& slot : handles) Require(sceAudio3dReportRegisterHandler(3, RecordReport, nullptr, &slot) == 0, "8 handlers fit in a type");
    for (int i = 1; i < 8; ++i) Require(handles[i] == handles[0] + 0x10 * i, "handles are consecutive slots");
    Require(sceAudio3dReportRegisterHandler(3, RecordReport, nullptr, &handle) == -3 && handle == 0xFFFFFFFF, "a full type writes the invalid handle");
    Require(sceAudio3dReportRegisterHandler(3, RecordReport, nullptr, nullptr) == -3, "a full type without a handle pointer");
    for (const std::uint64_t slot : handles) Require(sceAudio3dReportUnregisterHandler(slot) == 0, "unregister");
    Require(sceAudio3dReportUnregisterHandler(handles[0]) == -1, "double unregister");
    Require(sceAudio3dReportUnregisterHandler(0xFFFFFFFF) == -1, "unregister the invalid handle");
    Require(sceAudio3dReportUnregisterHandler(handles[0] + 1) == -1, "unregister an address inside a slot");
    Require(sceAudio3dReportRegisterHandler(3, RecordReport, nullptr, nullptr) == 0, "register without a handle pointer");
    Require(sceAudio3dReportRegisterHandler(3, RecordReport, nullptr, &handle) == 0 && handle == handles[1], "the first free slot is reused");
    Require(sceAudio3dReportUnregisterHandler(handles[0]) == 0 && sceAudio3dReportUnregisterHandler(handles[1]) == 0, "unregister type 3");

    std::uint64_t other = 0;
    Require(sceAudio3dReportRegisterHandler(1, RecordReport, nullptr, &other) == 0, "register a type 1 handler");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0, "open without a call handler");
    Require(g_reports.empty(), "only type 0 handlers receive call reports");
    Require(sceAudio3dPortClose(0) == 0, "close without a call handler");

    int user = 0;
    Require(sceAudio3dReportRegisterHandler(0, RecordReport, &user, &handle) == 0, "register a call handler");
    id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open with a call handler");
    Require(g_reports.size() == 1 && g_reports[0].user_data == &user, "the open is reported with the user data");
    RequireReport(g_reports[0], "sceAudio3dPortOpen", "I&HD", 0, OBJECT_INVALID, 0, {0xFF, Address(&id), Address(&parameters)}, "open report");
    g_reports.clear();
    Require(sceAudio3dPortGetStatus(1, nullptr, nullptr, 0) == INVALID_PORT, "status of port 1");
    RequireReport(g_reports.at(0), "sceAudio3dPortGetStatus", "H", 1, OBJECT_INVALID, INVALID_PORT, {1}, "failed status report");
    g_reports.clear();
    std::uint32_t object = 0;
    Require(sceAudio3dObjectReserve(0, &object) == 0, "reserve");
    RequireReport(g_reports.at(0), "sceAudio3dObjectReserve", "H&O", 0, object, 0, {0, Address(&object)}, "reserve report");
    g_reports.clear();
    const std::int16_t samples[8] = {};
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "bed write");
    Require(g_reports.size() == 2, "bed write reports itself and bed write2");
    RequireReport(g_reports[0], "sceAudio3dBedWrite2", "HUUPU", 0, OBJECT_INVALID, 0, {0, 2, 1, Address(samples), 4}, "bed write2 reports the AudioOut2 format");
    RequireReport(g_reports[1], "sceAudio3dBedWrite", "HUUPU", 0, OBJECT_INVALID, 0, {0, 2, 0, Address(samples), 4}, "bed write report");
    g_reports.clear();
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 1, false) == INVALID_PARAMETER, "bed write2 with a PS4 route");
    RequireReport(g_reports.at(0), "sceAudio3dBedWrite2", "HUUPU", 0, OBJECT_INVALID, INVALID_PARAMETER, {0, 2, 0, Address(samples), 4}, "a rejected bed keeps its format");
    g_reports.clear();
    Require(sceAudio3dAudioOutOutputs(nullptr, 0) == INVALID_PARAMETER, "outputs without parameters");
    RequireReport(g_reports.at(0), "sceAudio3dAudioOutOutputs", "PU", 0xFFFFFFFF, OBJECT_INVALID, INVALID_PARAMETER, {0, 0}, "outputs without parameters reports a null pointer");
    const int out_handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, parameters.granularity, 48000, 1);
    Require(out_handle > 0, "audio out open with a call handler");
    g_reports.clear();
    const std::vector<std::int16_t> block(static_cast<std::size_t>(parameters.granularity) * 2, 0);
    AudioOutOutputParam batch[2] = {{out_handle, block.data()}, {0x1234, block.data()}};
    Require(sceAudio3dAudioOutOutputs(batch, 2) == INVALID_PORT, "outputs with an unknown second handle");
    Require(g_reports.size() == 3, "outputs reports each output and itself");
    RequireReport(g_reports[0], "sceAudio3dAudioOutOutput", "IP", 0xFFFFFFFF, OBJECT_INVALID, 0, {static_cast<std::uint64_t>(out_handle), Address(block.data())}, "first output report");
    RequireReport(g_reports[1], "sceAudio3dAudioOutOutput", "IP", 0xFFFFFFFF, OBJECT_INVALID, INVALID_PORT, {0x1234, Address(block.data())}, "failed output report");
    RequireReport(g_reports[2], "sceAudio3dAudioOutOutputs", "PU", 0xFFFFFFFF, OBJECT_INVALID, INVALID_PORT, {Address(batch + 1), 2}, "outputs reports the element where it stopped");
    Require(sceAudio3dAudioOutClose(out_handle) == 0, "close the audio out handle");
    g_reports.clear();
    Require(sceAudio3dAudioOutClose(0x1234) == INVALID_PORT && g_reports.empty(), "closing an unknown audio out port is not reported");
    Require(sceAudio3dPortDestroy(0) == 0, "destroy");
    Require(g_reports.size() == 2, "destroy reports itself and the close");
    RequireReport(g_reports[0], "sceAudio3dPortClose", "H", 0, OBJECT_INVALID, 0, {0}, "close report");
    RequireReport(g_reports[1], "sceAudio3dPortDestroy", "H", 0, OBJECT_INVALID, 0, {0}, "destroy report");
    g_reports.clear();
    Require(sceAudio3dPortDestroy(0) == INVALID_PORT && g_reports.size() == 1, "destroying a closed port does not close it again");
    g_reports.clear();
    Require(sceAudio3dReportUnregisterHandler(handle) == 0 && sceAudio3dReportUnregisterHandler(other) == 0, "unregister the handlers");
    Require(sceAudio3dPortGetStatus(0, nullptr, nullptr, 0) == INVALID_PORT && g_reports.empty(), "no reports without a call handler");
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

void CheckPs4Process() {
    Require(sceAudio3dInitialize(0) == 0, "initialize in a PS4 process");
    Require(sceAudio3dInitialize(1) == NOT_READY, "a repeated initialize in a PS4 process is NOT_READY before the reserved check");
    Require(sceAudio3dInitialize(0) == NOT_READY, "a repeated initialize in a PS4 process is NOT_READY");
    const Audio3dOpenParameters defaults = Defaults();
    std::uint32_t id = 7;
    auto open = [&](auto change) {
        Audio3dOpenParameters parameters = defaults;
        change(parameters);
        return Open(parameters, &id);
    };
    RequireThrows([&] { open([](auto& p) { p.size_this = 0x10; }); }, "a PS4 process with 0x10 parameters selects the unsupported buffer mode 0");
    RequireThrows([&] { open([](auto& p) { p.size_this = 0x18; }); }, "a PS4 process with 0x18 parameters selects the unsupported buffer mode 1");
    RequireThrows([&] { open([](auto& p) { p.buffer_mode = 0; }); }, "a PS4 process with buffer mode 0 is unsupported");
    RequireThrows([&] { open([](auto& p) { p.buffer_mode = 1; }); }, "a PS4 process with buffer mode 1 is unsupported");
    RequireThrows([&] { open([](auto& p) { p.size_this = 0x28; p.num_beds = 3; }); }, "a PS4 process with 3 beds is unsupported");
    for (const std::uint32_t beds : {0u, 1u, 4u}) {
        Require(open([&](auto& p) { p.size_this = 0x28; p.num_beds = beds; }) == INVALID_PARAMETER && id == 7, "a PS4 process needs 2 or 3 beds");
    }
    Require(sceAudio3dPortOpenEx(1, &defaults, &id) == INVALID_PARAMETER && id == 7, "open ex in a PS4 process checks like open");
    Require(sceAudio3dPortOpenEx(SYSTEM_USER, &defaults, &id) == 0 && id == 0, "open ex in a PS4 process opens the port");
    id = 7;
    Require(open([](auto& p) { p.buffer_mode = 0; }) == OUT_OF_RESOURCES && id == 7, "a PS4 process reports the open port before an unsupported buffer mode");
    Require(sceAudio3dPortClose(0) == 0, "close after open ex in a PS4 process");
    Require(sceAudio3dPortCreate(0x100, 0, 1, &id) == INVALID_PARAMETER && id == 7, "create in a PS4 process needs a zero reserved argument");
    RequireThrows([&] { sceAudio3dPortCreate(0x100, 0, 0, &id); }, "create in a PS4 process selects the unsupported buffer mode 0");
    Require(open([](auto& p) { p.size_this = 0x28; p.num_beds = 2; }) == 0 && id == 0, "a PS4 process opens with 2 beds");
    const std::int16_t samples[8] = {};
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 1, false) == INVALID_PARAMETER, "a PS4 process rejects the hmu only route on a 2-bed port");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 2, false) == 0, "a PS4 process accepts the tv only route");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 3, false) == INVALID_PARAMETER, "a PS4 process rejects an unknown route");
    const std::uint32_t granularity = defaults.granularity;
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 3, 0, granularity, 48000, 1 | (1u << 17)) == AUDIO_OUT_INVALID_FORMAT, "a PS4 process rejects param bit 17 on port type 3");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 5, 0, granularity, 48000, 1) == AUDIO_OUT_INVALID_PORT_TYPE, "a PS4 process checks the port type");
    Require(sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 8) == AUDIO_OUT_INVALID_FORMAT, "a PS4 process rejects an unknown format");
    RequireThrows([&] { sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 6); }, "a PS4 process with format 6 depends on the SDK version");
    RequireThrows([&] { sceAudio3dAudioOutOpen(0, SYSTEM_USER, 0, 0, granularity, 48000, 7); }, "a PS4 process with format 7 depends on the SDK version");
    const int handle = sceAudio3dAudioOutOpen(0, SYSTEM_USER, 3, 0, granularity, 48000, 1);
    Require(handle > 0 && sceAudio3dAudioOutClose(handle) == 0, "a PS4 process opens port type 3 without bit 17");
    Require(sceAudio3dPortClose(0) == 0, "close in a PS4 process");
    Require(sceAudio3dTerminate() == 0, "terminate in a PS4 process");
}

int RunAsPs4Process(const char* program) {
    const auto directory = std::filesystem::temp_directory_path() / ("audio3d-ps4-test-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    struct Entry { std::uint16_t key, format; std::uint32_t length, maximum, offset; };
    const std::uint32_t header[5] = {0x46535000, 0x0101, 52, 72, 2};
    const Entry entries[2] = {{0, 0x0204, 8, 8, 0}, {6, 0x0204, 10, 12, 8}};
    {
        std::ofstream file(directory / "app0/sce_sys/param.sfo", std::ios::binary);
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(entries), sizeof(entries));
        file << std::string("TITLE\0TITLE_ID\0\0\0\0\0\0", 20) << std::string("Example\0", 8) << "CUSA00000" << std::string("\0\0\0", 3);
    }
#ifdef _WIN32
    const std::string command = "\"cd /d \"" + directory.string() + "\" && \"" + program + "\" ps4-process\"";
#else
    const std::string command = "cd \"" + directory.string() + "\" && \"" + program + "\" ps4-process";
#endif
    const int status = std::system(command.c_str());
    std::filesystem::remove_all(directory);
    return status;
}

}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "ps4-process") {
        CheckPs4Process();
        return 0;
    }
    CheckBeforeInitialize();
    CheckOpenParameters();
    CheckOpenClose();
    CheckPortWrappers();
    CheckObjects();
    CheckQueue();
    CheckMixAndPush();
    CheckPushKeepsRealTime();
    CheckFlush();
    CheckAudioOutPorts();
    CheckObjectAttributes();
    CheckBed();
    CheckBedQueue();
    CheckReportHandlers();
    CheckTerminate();
    Require(RunAsPs4Process(argv[0]) == 0, "the PS4 process checks must pass");
}

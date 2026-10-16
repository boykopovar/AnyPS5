#include "SceTypes.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <thread>
#include <vector>

extern "C" {
void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* parameters);
int APS5_VABI sceAudio3dInitialize(std::int64_t reserved);
int APS5_VABI sceAudio3dTerminate();
int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id);
int APS5_VABI sceAudio3dPortClose(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortSetAttribute(std::uint32_t port_id, std::uint32_t attribute_id, const void* attribute, std::size_t attribute_size);
int APS5_VABI sceAudio3dPortGetAttributesSupported(std::uint32_t port_id, std::uint32_t* attributes, std::uint32_t* num_attributes);
int APS5_VABI sceAudio3dPortGetQueueLevel(std::uint32_t port_id, std::uint32_t* queue_level, std::uint32_t* queue_available);
int APS5_VABI sceAudio3dPortAdvance(std::uint32_t port_id);
int APS5_VABI sceAudio3dPortPush(std::uint32_t port_id, std::uint32_t blocking);
int APS5_VABI sceAudio3dPortFlush(std::uint32_t port_id);
int APS5_VABI sceAudio3dObjectReserve(std::uint32_t port_id, std::uint32_t* object_id);
int APS5_VABI sceAudio3dObjectUnreserve(std::uint32_t port_id, std::uint32_t object_id);
int APS5_VABI sceAudio3dObjectSetAttributes(std::uint32_t port_id, std::uint32_t object_id, std::uint64_t num_attributes, const Audio3dAttribute* attribute_array);
int APS5_VABI sceAudio3dBedWrite(std::uint32_t port_id, std::uint32_t num_channels, std::uint32_t format, const void* buffer, std::uint32_t num_samples);
int APS5_VABI sceAudio3dBedWrite2(std::uint32_t port_id, std::uint32_t num_channels, std::uint32_t format, const void* buffer, std::uint32_t num_samples, std::uint32_t output_route, bool restricted);
int APS5_VABI sceAudio3dAudioOutOpen(std::uint32_t port_id, int user_id, std::int32_t type, std::int32_t index, std::uint32_t len, std::uint32_t freq, std::uint32_t param);
int APS5_VABI sceAudio3dAudioOutClose(std::int32_t handle);
int APS5_VABI sceAudio3dAudioOutOutput(std::int32_t handle, const void* ptr);
int APS5_VABI sceAudio3dAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num);
int APS5_VABI sceAudio3dObjectSetAttribute(std::uint32_t port_id, std::uint32_t object_id, std::uint32_t attribute_id, const void* attribute, std::uint64_t attribute_size);
int APS5_VABI sceAudio3dPortGetParameters(std::uint32_t port_id, Audio3dOpenParameters* out);
const char* APS5_VABI sceAudio3dStrError(int error_code);
int APS5_VABI sceAudio3dPortGetStatus(void);
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
    std::uint32_t num_attributes = 3;
    std::uint32_t attributes[3]{};
    Require(sceAudio3dPortGetAttributesSupported(0, attributes, &num_attributes) == INVALID_PORT, "supported attributes need an open port");
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
    constexpr std::uint32_t supported[]{0x10001, 0x10002, 0x10003};
    std::uint32_t num_attributes = 99;
    Require(sceAudio3dPortGetAttributesSupported(0, nullptr, &num_attributes) == 0 && num_attributes == 3, "null output reports the supported attribute count");
    Require(sceAudio3dPortGetAttributesSupported(1, nullptr, &num_attributes) == INVALID_PORT, "supported attributes reject an invalid port");
    Require(sceAudio3dPortGetAttributesSupported(0, nullptr, nullptr) == INVALID_PARAMETER, "supported attributes require a count pointer");
    num_attributes = 2;
    std::uint32_t partial[3]{0, 0, 99};
    Require(sceAudio3dPortGetAttributesSupported(0, partial, &num_attributes) == 0 && num_attributes == 2, "supported attributes respect the caller capacity");
    Require(partial[0] == supported[0] && partial[1] == supported[1] && partial[2] == 99, "supported attributes copy only the requested entries");
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
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 4) == 0, "2 channel s16 bed write");
    Require(sceAudio3dBedWrite(0, 1, 0, samples, 4) == INVALID_PARAMETER, "1 channel bed write");
    Require(sceAudio3dBedWrite(0, 2, 0, nullptr, 4) == INVALID_PARAMETER, "null bed buffer");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 0, false) == 0, "bed write2 route both");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 1, false) == 0, "bed write2 route hmu only");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 2, false) == 0, "bed write2 route tv only");
    Require(sceAudio3dBedWrite2(0, 2, 0, samples, 4, 3, false) == INVALID_PARAMETER, "bed write2 unknown route");
    Require(sceAudio3dBedWrite(0, 2, 2, samples, 4) == INVALID_PARAMETER, "unknown bed format");
    Require(sceAudio3dBedWrite(0, 2, 0, samples, 0) == INVALID_PARAMETER, "zero bed samples");
    Require(sceAudio3dBedWrite(0, 2, 0, reinterpret_cast<const char*>(samples) + 1, 4) == INVALID_PARAMETER, "misaligned s16 bed buffer");
    const float floats[8] = {};
    Require(sceAudio3dBedWrite(0, 2, 1, floats, 4) == 0, "float bed write");
    const std::uint32_t granularity = parameters.granularity;
    std::vector<std::int16_t> wide(static_cast<std::size_t>(granularity) * 8, 0);
    Require(sceAudio3dBedWrite(0, 6, 0, wide.data(), granularity) == 0, "6 channel bed write");
    Require(sceAudio3dBedWrite(0, 8, 0, wide.data(), granularity) == 0, "8 channel bed write");
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

void CheckRemainingExports() {
    float value = 0.5f;
    Require(sceAudio3dObjectSetAttribute(0, 1, 0x10001, &value, sizeof(value)) == INVALID_PORT, "object set attribute needs an open port");
    Audio3dOpenParameters parameters = Defaults();
    std::uint32_t id = 7;
    Require(Open(parameters, &id) == 0 && id == 0, "open");
    std::uint32_t object = 0;
    Require(sceAudio3dObjectReserve(0, &object) == 0 && object == 1, "reserve");
    Require(sceAudio3dObjectSetAttribute(1, 1, 0x10001, &value, sizeof(value)) == INVALID_PORT, "object set attribute on port 1");
    Require(sceAudio3dObjectSetAttribute(0, 1, 0x10001, &value, sizeof(value)) == 0, "object set attribute on a reserved object");
    Require(sceAudio3dObjectSetAttribute(0, 2, 0x10001, &value, sizeof(value)) == INVALID_OBJECT, "object set attribute on an unreserved object");
    Require(sceAudio3dPortGetParameters(1, nullptr) == INVALID_PORT, "get parameters on port 1");
    Require(sceAudio3dPortGetParameters(0, nullptr) == INVALID_PARAMETER, "get parameters needs an output");
    Audio3dOpenParameters out;
    std::memset(&out, 0xAA, sizeof(out));
    Require(sceAudio3dPortGetParameters(0, &out) == 0, "get parameters");
    Require(out.size_this == 0x28, "the stored parameters use the full size");
    Require(out.granularity == parameters.granularity && out.rate == parameters.rate, "the stored granularity and rate");
    Require(out.max_objects == parameters.max_objects && out.queue_depth == parameters.queue_depth, "the stored object and queue limits");
    Require(out.buffer_mode == parameters.buffer_mode && out.pad == 0 && out.num_beds == 2, "the stored buffer mode, padding and beds");
    const char* invalid_port = sceAudio3dStrError(INVALID_PORT);
    Require(invalid_port != nullptr && invalid_port[0] != '\0', "str error returns a non-empty string");
    const char* unknown = sceAudio3dStrError(0);
    Require(unknown != nullptr && unknown[0] != '\0', "str error returns a string for an unknown code");
    RequireThrows([] { sceAudio3dPortGetStatus(); }, "an unimplemented export throws");
    Require(sceAudio3dPortClose(0) == 0, "close");
}

}

int main() {
    CheckBeforeInitialize();
    CheckOpenParameters();
    CheckOpenClose();
    CheckObjects();
    CheckQueue();
    CheckMixAndPush();
    CheckFlush();
    CheckAudioOutPorts();
    CheckObjectAttributes();
    CheckBed();
    CheckRemainingExports();
    CheckTerminate();
}

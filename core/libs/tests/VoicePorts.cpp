#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
int APS5_VABI sceVoiceInit(VoiceInitParam* param, std::int32_t version);
int APS5_VABI sceVoiceEnd_nid_postfix();
int APS5_VABI sceVoiceStart(const VoiceStartParam* param);
int APS5_VABI sceVoiceStop();
int APS5_VABI sceVoiceCreatePort(std::uint32_t* port_id, const VoicePortParam* param);
int APS5_VABI sceVoiceDeletePort(std::uint32_t port_id);
int APS5_VABI sceVoiceConnectIPortToOPort(std::uint32_t input_port_id, std::uint32_t output_port_id);
int APS5_VABI sceVoiceDisconnectIPortFromOPort(std::uint32_t input_port_id, std::uint32_t output_port_id);
int APS5_VABI sceVoiceWriteToIPort(std::uint32_t input_port_id, const void* data, std::uint32_t* size, std::int16_t frame_gaps);
int APS5_VABI sceVoiceReadFromOPort(std::uint32_t output_port_id, void* data, std::uint32_t* size);
int APS5_VABI sceVoiceGetPortInfo(std::uint32_t port_id, VoicePortInfo* info);
int APS5_VABI sceVoiceGetVolume(std::uint32_t port_id, float* volume);
int APS5_VABI sceVoiceSetVolume(std::uint32_t port_id, float volume);
int APS5_VABI sceVoiceGetBitRate(std::uint32_t port_id, std::uint32_t* bitrate);
int APS5_VABI sceVoiceGetPortAttr(std::uint32_t port_id, std::int32_t attr, void* value, std::int32_t size);
}

namespace {

constexpr std::int32_t PortTypeInPcmAudio = 1;
constexpr std::int32_t PortTypeOutPcmAudio = 3;
constexpr std::int32_t PortTypeInVoice = 2;

int failures = 0;

void Require(const bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "VoicePorts: %s\n", what);
    ++failures;
}

VoicePortParam MakePort(const std::int32_t type) {
    VoicePortParam param{};
    param.port_type = type;
    param.volume = 1.0f;
    param.pcmaudio.buffer_size = 4096;
    param.pcmaudio.sample_rate = 16000;
    return param;
}

}

int main() {
    std::uint32_t port = 0;
    const auto input = MakePort(PortTypeInPcmAudio);
    Require(sceVoiceCreatePort(&port, &input) != 0, "a port was created before the library was initialised");

    VoiceInitParam init{};
    Require(sceVoiceInit(&init, 100) == 0, "the library did not initialise");
    Require(sceVoiceInit(nullptr, 100) != 0, "a null init parameter was accepted");

    std::uint32_t in = 0;
    std::uint32_t out = 0;
    const auto output = MakePort(PortTypeOutPcmAudio);
    Require(sceVoiceCreatePort(&in, &input) == 0, "the input port was refused");
    Require(sceVoiceCreatePort(&out, &output) == 0, "the output port was refused");
    Require(in != out, "two ports share an identifier");

    const auto encoded = MakePort(PortTypeInVoice);
    std::uint32_t unsupported = 0;
    Require(sceVoiceCreatePort(&unsupported, &encoded) != 0, "an encoded port was accepted without a codec");

    Require(sceVoiceConnectIPortToOPort(out, in) != 0, "an output was connected to an input");
    Require(sceVoiceConnectIPortToOPort(in, out) == 0, "the ports were not connected");
    Require(sceVoiceConnectIPortToOPort(in, out) != 0, "the same edge was created twice");

    const std::uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::uint32_t size = sizeof(payload);
    Require(sceVoiceWriteToIPort(in, payload, &size, 0) == 0 && size == sizeof(payload), "the payload was not written");

    VoicePortInfo info{};
    Require(sceVoiceGetPortInfo(out, &info) == 0, "the port info was refused");
    Require(info.byte_count == sizeof(payload), "the output does not hold the written bytes");
    Require(info.edge_count == 1, "the output does not count its source");

    std::uint8_t received[8] = {};
    std::uint32_t readSize = sizeof(received);
    Require(sceVoiceReadFromOPort(out, received, &readSize) == 0, "the payload was not read back");
    Require(readSize == sizeof(payload) && std::memcmp(received, payload, sizeof(payload)) == 0, "the payload changed on the way");

    readSize = sizeof(received);
    Require(sceVoiceReadFromOPort(out, received, &readSize) == 0 && readSize == 0, "the queue was not drained");

    std::uint32_t partial = 3;
    size = sizeof(payload);
    Require(sceVoiceWriteToIPort(in, payload, &size, 0) == 0, "the second write failed");
    Require(sceVoiceReadFromOPort(out, received, &partial) == 0 && partial == 3, "a short read did not return three bytes");
    Require(sceVoiceGetPortInfo(out, &info) == 0 && info.byte_count == 5, "the rest of the payload was lost");

    Require(sceVoiceSetVolume(in, 0.5f) == 0, "the volume was refused");
    float volume = 0.0f;
    Require(sceVoiceGetVolume(in, &volume) == 0 && volume == 0.5f, "the volume did not stick");
    Require(sceVoiceSetVolume(in, -1.0f) != 0, "a negative volume was accepted");
    float attribute = 0.0f;
    Require(sceVoiceGetPortAttr(in, 0, &attribute, sizeof(attribute)) == 0 && attribute == 0.5f, "the volume attribute disagrees");
    Require(sceVoiceGetPortAttr(in, 99, &attribute, sizeof(attribute)) != 0, "an unknown attribute was served");

    std::uint32_t bitrate = 0;
    Require(sceVoiceGetBitRate(in, &bitrate) == 0 && bitrate > 0, "the bit rate is not reported");

    VoiceStartParam start{};
    Require(sceVoiceStart(&start) == 0, "the library did not start");
    Require(sceVoiceGetPortInfo(out, &info) == 0 && info.state == 3, "a started port is not running");
    Require(sceVoiceStop() == 0, "the library did not stop");

    Require(sceVoiceDisconnectIPortFromOPort(in, out) == 0, "the edge was not removed");
    Require(sceVoiceDisconnectIPortFromOPort(in, out) != 0, "a missing edge was removed twice");
    size = sizeof(payload);
    Require(sceVoiceWriteToIPort(in, payload, &size, 0) == 0 && size == 0, "a disconnected input still delivers");

    Require(sceVoiceDeletePort(in) == 0, "the input port was not deleted");
    Require(sceVoiceDeletePort(in) != 0, "the port was deleted twice");
    Require(sceVoiceGetPortInfo(out, &info) == 0 && info.edge_count == 0, "the output kept a dangling source");

    std::uint32_t second = 0;
    Require(sceVoiceCreatePort(&second, &input) == 0, "the second input port was refused");
    Require(sceVoiceConnectIPortToOPort(second, out) == 0, "the second input was not connected");
    Require(sceVoiceGetPortInfo(second, &info) == 0 && info.edge_count == 1, "the second input has no edge");
    Require(sceVoiceDeletePort(out) == 0, "the output port was not deleted");
    Require(sceVoiceGetPortInfo(second, &info) == 0 && info.edge_count == 0, "the input kept an edge to a deleted output");
    size = sizeof(payload);
    Require(sceVoiceWriteToIPort(second, payload, &size, 0) == 0 && size == 0, "writing to an input whose output is gone delivered bytes");

    Require(sceVoiceEnd_nid_postfix() == 0, "the library did not shut down");
    Require(sceVoiceGetPortInfo(second, &info) != 0, "a port outlived the library");

    if (failures != 0) return 1;
    std::printf("Voice port tests passed\n");
    return 0;
}

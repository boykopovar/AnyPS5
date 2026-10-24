#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct Item {
    std::uint32_t size;
    void* address;
    std::uint32_t bytes;
};

struct Ctrl {
    void* param;
    void* bsiInfo;
    Item* au;
    Item* pcm;
};

struct LpcmParam {
    std::uint32_t size, channelConfig, rateIndex, bitsIndex, auBytes, reserved;
    std::uint64_t header;
    std::uint32_t ptsHigh, ptsLow;
};

struct LpcmBsiInfo {
    std::uint32_t reserved, channelConfig, rateIndex, pcmBytes;
    std::uint64_t header;
    std::uint32_t ptsHigh, ptsLow;
};

}

extern "C" {
int APS5_VABI sceAudiodecCpuQueryMemSize(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuInitDecoder(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuDecode(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuClearContext(Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuInternalQueryMemSize(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuInternalInitDecoder(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuInternalDecode(Ctrl*, Item*, std::uint32_t);
int APS5_VABI sceAudiodecCpuInternalClearContext(Item*, std::uint32_t);
}

namespace {

constexpr std::uint32_t LpcmBd = 0x1003;
constexpr std::uint32_t LpcmDvd = 0x1004;
constexpr std::uint32_t Trhd = 0x100b;
constexpr std::uint32_t None = 0xffffffffu;

void Require(bool value) { if (!value) std::abort(); }

int Error(std::uint32_t code) { return static_cast<int>(0x80f70000u + code); }

std::uint64_t Header(std::uint32_t channels, std::uint32_t rate, std::uint32_t bits, std::uint32_t frames) {
    return static_cast<std::uint64_t>(channels) << 32 | static_cast<std::uint64_t>(rate) << 40 | static_cast<std::uint64_t>(bits) << 48 | static_cast<std::uint64_t>(frames) << 56;
}

struct Decoder {
    std::vector<std::uint8_t> work = std::vector<std::uint8_t>(0x190138);
    std::vector<std::uint8_t> input = std::vector<std::uint8_t>(0x2000);
    std::vector<float> output = std::vector<float>(0x1000);
    LpcmParam param{0x28, 0, 0, 0, 0, 0, Header(2, 1, 1, 1), None, None};
    LpcmBsiInfo info{};
    Item au{0x18, input.data(), static_cast<std::uint32_t>(input.size())};
    Item pcm{0x18, output.data(), static_cast<std::uint32_t>(output.size() * 4)};
    Item memory{0x18, work.data(), 0};
    Ctrl ctrl{&param, &info, &au, &pcm};

    int Decode(std::uint32_t high, std::uint32_t low) {
        param.ptsHigh = high;
        param.ptsLow = low;
        au.bytes = static_cast<std::uint32_t>(input.size());
        pcm.bytes = static_cast<std::uint32_t>(output.size() * 4);
        return sceAudiodecCpuDecode(&ctrl, &memory, LpcmBd);
    }
};

void TestQueryMemSize() {
    Decoder decoder;
    Require(sceAudiodecCpuQueryMemSize(nullptr, &decoder.memory, LpcmBd) == Error(9));
    Ctrl noParam{};
    Require(sceAudiodecCpuQueryMemSize(&noParam, &decoder.memory, LpcmBd) == Error(0xa));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    Item badSize{0x10, nullptr, 0};
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &decoder.memory, 0x1009) == Error(1));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &decoder.memory, 0x1000) == Error(1));
    Item empty{0x18, nullptr, 0};
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &empty, LpcmBd) == 0);
    Require(empty.bytes == 0x190138);
    empty.bytes = 0;
    Require(sceAudiodecCpuInternalQueryMemSize(&decoder.ctrl, &empty, LpcmDvd) == 0);
    Require(empty.bytes == 0x190138);
}

void TestInitAndClear() {
    Decoder decoder;
    Ctrl noBsi{&decoder.param, nullptr, &decoder.au, &decoder.pcm};
    Require(sceAudiodecCpuInitDecoder(&noBsi, &decoder.memory, LpcmBd) == Error(0xb));
    Item noAddress{0x18, nullptr, 0};
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &noAddress, LpcmBd) == Error(0x11));
    Item badSize{0x10, decoder.work.data(), 0};
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, 0x100a) == Error(1));
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(sceAudiodecCpuInternalInitDecoder(&decoder.ctrl, &decoder.memory, LpcmDvd) == 0);
    Require(sceAudiodecCpuClearContext(nullptr, LpcmBd) == Error(2));
    Require(sceAudiodecCpuClearContext(&noAddress, LpcmBd) == Error(0x11));
    Require(sceAudiodecCpuClearContext(&badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuClearContext(&decoder.memory, 0x1009) == Error(1));
    Require(sceAudiodecCpuClearContext(&decoder.memory, LpcmBd) == 0);
    Require(sceAudiodecCpuInternalClearContext(&decoder.memory, LpcmDvd) == 0);
}

void TestDecodeArguments() {
    Decoder decoder;
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(sceAudiodecCpuDecode(nullptr, &decoder.memory, LpcmBd) == Error(9));
    Ctrl ctrl = decoder.ctrl;
    ctrl.bsiInfo = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xb));
    ctrl = decoder.ctrl;
    ctrl.au = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xc));
    Item au{0x18, nullptr, 0};
    ctrl = decoder.ctrl;
    ctrl.au = &au;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xf));
    ctrl.pcm = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, Trhd) == Error(0xd));
    Item pcm{0x18, nullptr, 4};
    ctrl.pcm = &pcm;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, Trhd) == Error(0x10));
    au.address = decoder.input.data();
    pcm.address = decoder.output.data();
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0x13));
    au.bytes = 4;
    pcm.bytes = 0;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0x14));
    pcm.bytes = 4;
    au.size = 0x10;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(6));
    au.size = 0x18;
    pcm.size = 0x10;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(7));
    pcm.size = 0x18;
    Require(sceAudiodecCpuDecode(&ctrl, nullptr, LpcmBd) == Error(0xe));
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, 0x1009) == Error(1));
    decoder.param.size = 0x20;
    Require(decoder.Decode(None, None) == static_cast<int>(0x80f70100u));
    decoder.param.size = 0x28;
    for (const std::uint64_t header : {Header(0, 1, 1, 1), Header(11, 1, 1, 1), Header(2, 0, 1, 1), Header(2, 4, 1, 1), Header(2, 1, 0, 1), Header(2, 1, 4, 1)}) {
        decoder.param.header = header;
        Require(decoder.Decode(None, None) == static_cast<int>(0x80f70100u));
    }
}

void TestDecodeSamples() {
    Decoder decoder;
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    std::memset(decoder.input.data(), 0, decoder.input.size());
    const std::uint8_t samples16[] = {0x80, 0x00, 0x40, 0x00, 0xff, 0xff, 0x7f, 0xff};
    std::memcpy(decoder.input.data(), samples16, sizeof(samples16));
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.au.bytes == 960 && decoder.pcm.bytes == 480 * 4);
    Require(decoder.param.channelConfig == 2 && decoder.param.rateIndex == 1 && decoder.param.bitsIndex == 1 && decoder.param.auBytes == 960);
    Require(decoder.output[0] == -1.0f && decoder.output[1] == 0.5f && decoder.output[2] == -0x1p-15f && decoder.output[3] == 0x7fff0000 * 0x1p-31f && decoder.output[4] == 0.0f);
    Require(decoder.info.channelConfig == 2 && decoder.info.rateIndex == 1 && decoder.info.pcmBytes == 480 * 4 && decoder.info.header == Header(2, 1, 1, 1));
    decoder.param.header = Header(2, 2, 2, 2);
    const std::uint8_t samples24[] = {0x00, 0x00, 0x01, 0xff, 0xff, 0xff};
    std::memcpy(decoder.input.data(), samples24, sizeof(samples24));
    decoder.au.bytes = static_cast<std::uint32_t>(decoder.input.size());
    decoder.pcm.bytes = static_cast<std::uint32_t>(decoder.output.size() * 4);
    Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(decoder.param.auBytes == 5760 && decoder.au.bytes == 5760 && decoder.pcm.bytes == 1920 * 4);
    Require(decoder.output[0] == 0x1p-23f && decoder.output[1] == -0x1p-23f);
    Require(decoder.info.rateIndex == 2);
}

void TestTimestamps() {
    Decoder decoder;
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == None && decoder.info.ptsLow == None);
    Require(decoder.Decode(0, 1000) == 0);
    Require(decoder.info.ptsHigh == 0 && decoder.info.ptsLow == 1000);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == 0 && decoder.info.ptsLow == 1450);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == 0 && decoder.info.ptsLow == 1900);
    Require(decoder.Decode(0, 0xffffff00u) == 0);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == 1 && decoder.info.ptsLow == 0xc2);
    Require(decoder.Decode(1, 0xffffff00u) == 0);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == 0 && decoder.info.ptsLow == 0xc2);
    Require(sceAudiodecCpuClearContext(&decoder.memory, LpcmBd) == 0);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == None && decoder.info.ptsLow == None);
    Require(decoder.Decode(7, 9) == 0);
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.info.ptsHigh == None && decoder.info.ptsLow == None);
}

}

int main() {
    TestQueryMemSize();
    TestInitAndClear();
    TestDecodeArguments();
    TestDecodeSamples();
    TestTimestamps();
    return 0;
}

#include "prx/libSceAudiodecCpu/libSceAudiodecCpu.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

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
    std::vector<std::uint8_t> work;
    std::vector<std::uint8_t> input;
    std::vector<float> output;
    LpcmParam param;
    LpcmBsiInfo info;
    AudiodecAuInfo au;
    AudiodecPcmItem pcm;
    AudiodecCpuMemory memory;
    AudiodecCtrl ctrl;

    explicit Decoder(std::uint32_t inputBytes = 0x2000, std::uint32_t outputFloats = 0x1000)
        : work(0x190138),
          input(inputBytes),
          output(outputFloats),
          param{0x28, 0, 0, 0, 0, 0, Header(2, 1, 1, 1), None, None},
          info{},
          au{0x18, input.data(), inputBytes},
          pcm{0x18, output.data(), outputFloats * 4},
          memory{0x18, work.data(), 0},
          ctrl{&param, &info, &au, &pcm} {}

    int Decode(std::uint32_t high, std::uint32_t low) {
        param.ptsHigh = high;
        param.ptsLow = low;
        au.ui_au_size = static_cast<std::uint32_t>(input.size());
        pcm.ui_pcm_size = static_cast<std::uint32_t>(output.size() * 4);
        return sceAudiodecCpuDecode(&ctrl, &memory, LpcmBd);
    }
};

void TestQueryMemSize() {
    Decoder decoder;
    Require(sceAudiodecCpuQueryMemSize(nullptr, &decoder.memory, LpcmBd) == Error(9));
    AudiodecCtrl noParam{};
    Require(sceAudiodecCpuQueryMemSize(&noParam, &decoder.memory, LpcmBd) == Error(0xa));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    AudiodecCpuMemory badSize{0x10, nullptr, 0};
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &decoder.memory, 0x1009) == Error(1));
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &decoder.memory, 0x1000) == Error(1));
    AudiodecCpuMemory empty{0x18, nullptr, 0};
    Require(sceAudiodecCpuQueryMemSize(&decoder.ctrl, &empty, LpcmBd) == 0);
    Require(empty.ui_memory_size == 0x190138);
    empty.ui_memory_size = 0;
    Require(sceAudiodecCpuInternalQueryMemSize(&decoder.ctrl, &empty, LpcmDvd) == 0);
    Require(empty.ui_memory_size == 0x190138);
    AudiodecCtrl paramOnly{&decoder.param, nullptr, nullptr, nullptr};
    empty.ui_memory_size = 7;
    Require(sceAudiodecCpuQueryMemSize(&paramOnly, &empty, 0x1009) == Error(1));
    Require(empty.ui_memory_size == 7);
    Require(sceAudiodecCpuQueryMemSize(&paramOnly, &empty, LpcmBd) == 0);
    Require(empty.ui_memory_size == 0x190138);
}

void TestInitAndClear() {
    Decoder decoder;
    Require(sceAudiodecCpuInitDecoder(nullptr, &decoder.memory, LpcmBd) == Error(9));
    AudiodecCtrl noParam{nullptr, &decoder.info, &decoder.au, &decoder.pcm};
    Require(sceAudiodecCpuInitDecoder(&noParam, &decoder.memory, LpcmBd) == Error(0xa));
    AudiodecCtrl noBsi{&decoder.param, nullptr, &decoder.au, &decoder.pcm};
    Require(sceAudiodecCpuInitDecoder(&noBsi, &decoder.memory, LpcmBd) == Error(0xb));
    Require(sceAudiodecCpuInitDecoder(&noBsi, nullptr, LpcmBd) == Error(0xb));
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    AudiodecCpuMemory noAddress{0x18, nullptr, 0};
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &noAddress, LpcmBd) == Error(0x11));
    AudiodecCpuMemory badSize{0x10, decoder.work.data(), 0};
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &badSize, 0x100a) == Error(8));
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
    AudiodecCtrl ctrl = decoder.ctrl;
    ctrl.pParam = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xa));
    ctrl = decoder.ctrl;
    ctrl.pBsiInfo = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xb));
    ctrl = decoder.ctrl;
    ctrl.pAuInfo = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xc));
    AudiodecAuInfo au{0x18, nullptr, 0};
    ctrl = decoder.ctrl;
    ctrl.pAuInfo = &au;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0xf));
    ctrl.pPcmItem = nullptr;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, Trhd) == Error(0xd));
    AudiodecPcmItem pcm{0x18, nullptr, 4};
    ctrl.pPcmItem = &pcm;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, Trhd) == Error(0x10));
    au.p_au_addr = decoder.input.data();
    pcm.p_pcm_addr = decoder.output.data();
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0x13));
    au.ui_au_size = 4;
    pcm.ui_pcm_size = 0;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(0x14));
    pcm.ui_pcm_size = 4;
    au.ui_size = 0x10;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(6));
    au.ui_size = 0x18;
    pcm.ui_size = 0x10;
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, LpcmBd) == Error(7));
    pcm.ui_size = 0x18;
    Require(sceAudiodecCpuDecode(&ctrl, nullptr, LpcmBd) == Error(0xe));
    AudiodecCpuMemory noAddress{0x18, nullptr, 0};
    Require(sceAudiodecCpuDecode(&ctrl, &noAddress, LpcmBd) == Error(0x11));
    AudiodecCpuMemory badSize{0x10, decoder.work.data(), 0};
    Require(sceAudiodecCpuDecode(&ctrl, &badSize, 0x1009) == Error(8));
    Require(sceAudiodecCpuDecode(&ctrl, &decoder.memory, 0x1009) == Error(1));
    decoder.param.size = 0x20;
    Require(decoder.Decode(None, None) == static_cast<int>(0x80f70100u));
    decoder.param.size = 0x28;
    for (const std::uint64_t header : {Header(0, 1, 1, 1), Header(11, 1, 1, 1), Header(2, 0, 1, 1), Header(2, 4, 1, 1), Header(2, 1, 0, 1), Header(2, 1, 4, 1)}) {
        decoder.param.header = header;
        Require(decoder.Decode(None, None) == static_cast<int>(0x80f70100u));
    }
    Require(decoder.param.channelConfig == 0 && decoder.param.rateIndex == 0 && decoder.param.bitsIndex == 0 && decoder.param.auBytes == 0);
}

void TestDecodeSamples() {
    Decoder decoder;
    Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    std::memset(decoder.input.data(), 0, decoder.input.size());
    const std::uint8_t samples16[] = {0x80, 0x00, 0x40, 0x00, 0xff, 0xff, 0x7f, 0xff};
    std::memcpy(decoder.input.data(), samples16, sizeof(samples16));
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.au.ui_au_size == 960 && decoder.pcm.ui_pcm_size == 480 * 4);
    Require(decoder.param.channelConfig == 2 && decoder.param.rateIndex == 1 && decoder.param.bitsIndex == 1 && decoder.param.auBytes == 960);
    Require(decoder.output[0] == -1.0f && decoder.output[1] == 0.5f && decoder.output[2] == -0x1p-15f && decoder.output[3] == 0x7fff0000 * 0x1p-31f && decoder.output[4] == 0.0f);
    Require(decoder.info.channelConfig == 2 && decoder.info.rateIndex == 1 && decoder.info.pcmBytes == 480 * 4 && decoder.info.header == Header(2, 1, 1, 1));
    decoder.param.header = Header(2, 2, 2, 2);
    const std::uint8_t samples24[] = {0x00, 0x00, 0x01, 0xff, 0xff, 0xff};
    std::memcpy(decoder.input.data(), samples24, sizeof(samples24));
    decoder.au.ui_au_size = static_cast<std::uint32_t>(decoder.input.size());
    decoder.pcm.ui_pcm_size = static_cast<std::uint32_t>(decoder.output.size() * 4);
    Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
    Require(decoder.param.auBytes == 5760 && decoder.au.ui_au_size == 5760 && decoder.pcm.ui_pcm_size == 1920 * 4);
    Require(decoder.output[0] == 0x1p-23f && decoder.output[1] == -0x1p-23f);
    Require(decoder.info.rateIndex == 2);
    decoder.param.header = Header(2, 3, 3, 1);
    Require(decoder.Decode(None, None) == 0);
    Require(decoder.param.rateIndex == 3 && decoder.param.bitsIndex == 3 && decoder.param.auBytes == 5760);
    Require(decoder.au.ui_au_size == 5760 && decoder.pcm.ui_pcm_size == 1920 * 4 && decoder.info.rateIndex == 3);
}

std::uint16_t Pattern16(std::uint32_t index) {
    return static_cast<std::uint16_t>(index * 7919u + 0x1234u);
}

std::uint32_t Pattern24(std::uint32_t index) {
    return (index * 7919u + 0x1234u) & 0xffffffu;
}

float Expected16(std::uint32_t index) {
    return static_cast<float>(static_cast<std::int32_t>(static_cast<std::uint32_t>(Pattern16(index)) << 16)) * 0x1p-31f;
}

float Expected24(std::uint32_t index) {
    return static_cast<float>(static_cast<std::int32_t>(Pattern24(index) << 8)) * 0x1p-31f;
}

void Fill16(std::vector<std::uint8_t>& input) {
    for (std::uint32_t index = 0; index < input.size() / 2; ++index) {
        const std::uint16_t value = Pattern16(index);
        input[index * 2] = static_cast<std::uint8_t>(value >> 8);
        input[index * 2 + 1] = static_cast<std::uint8_t>(value);
    }
}

void Fill24(std::vector<std::uint8_t>& input) {
    for (std::uint32_t index = 0; index < input.size() / 3; ++index) {
        const std::uint32_t value = Pattern24(index);
        input[index * 3] = static_cast<std::uint8_t>(value >> 16);
        input[index * 3 + 1] = static_cast<std::uint8_t>(value >> 8);
        input[index * 3 + 2] = static_cast<std::uint8_t>(value);
    }
}

void TestDecodeWrap() {
    constexpr std::uint32_t OutputSamples = 0x43800u / 4u;
    {
        Decoder decoder(145 * 960, 0x12000);
        decoder.param.header = Header(2, 1, 1, 145);
        Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Fill16(decoder.input);
        for (std::uint32_t index = 145 * 480; index < decoder.output.size(); ++index) decoder.output[index] = 1.5f;
        Require(decoder.Decode(None, None) == 0);
        Require(decoder.param.auBytes == 139200 && decoder.au.ui_au_size == 139200);
        Require(decoder.pcm.ui_pcm_size == 145 * 480 * 4 && decoder.info.pcmBytes == 145 * 480 * 4);
        Require(decoder.info.channelConfig == 2 && decoder.info.rateIndex == 1 && decoder.info.header == Header(2, 1, 1, 145));
        for (std::uint32_t index = 0; index < 145 * 480; ++index)
            Require(decoder.output[index] == Expected16(index % OutputSamples));
        for (std::uint32_t index = 145 * 480; index < decoder.output.size(); ++index)
            Require(decoder.output[index] == 1.5f);
    }
    {
        Decoder decoder(144 * 960, 0x12000);
        decoder.param.header = Header(2, 1, 1, 144);
        Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Fill16(decoder.input);
        for (std::uint32_t index = 144 * 480; index < decoder.output.size(); ++index) decoder.output[index] = 1.5f;
        decoder.au.ui_au_size = static_cast<std::uint32_t>(decoder.input.size());
        decoder.pcm.ui_pcm_size = static_cast<std::uint32_t>(decoder.output.size() * 4);
        Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Require(decoder.param.auBytes == 138240 && decoder.pcm.ui_pcm_size == 276480 && decoder.info.pcmBytes == 276480);
        for (std::uint32_t index = 0; index < 144 * 480; ++index)
            Require(decoder.output[index] == Expected16(index));
        for (std::uint32_t index = 144 * 480; index < decoder.output.size(); ++index)
            Require(decoder.output[index] == 1.5f);
    }
    {
        Decoder decoder(143 * 960, 0x12000);
        decoder.param.header = Header(2, 1, 1, 143);
        Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Fill16(decoder.input);
        for (std::uint32_t index = 143 * 480; index < decoder.output.size(); ++index) decoder.output[index] = 1.5f;
        Require(decoder.Decode(None, None) == 0);
        Require(decoder.pcm.ui_pcm_size == 143 * 480 * 4);
        for (std::uint32_t index = 0; index < 143 * 480; ++index)
            Require(decoder.output[index] == Expected16(index));
        for (std::uint32_t index = 143 * 480; index < decoder.output.size(); ++index)
            Require(decoder.output[index] == 1.5f);
    }
    {
        Decoder decoder(255 * 960, 0x20000);
        decoder.param.header = Header(2, 1, 1, 255);
        Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Fill16(decoder.input);
        for (std::uint32_t index = 255 * 480; index < decoder.output.size(); ++index) decoder.output[index] = 1.5f;
        Require(decoder.Decode(None, None) == 0);
        Require(decoder.param.auBytes == 244800 && decoder.pcm.ui_pcm_size == 255 * 480 * 4);
        for (std::uint32_t index = 0; index < 255 * 480; ++index)
            Require(decoder.output[index] == Expected16(index % OutputSamples));
        for (std::uint32_t index = 255 * 480; index < decoder.output.size(); ++index)
            Require(decoder.output[index] == 1.5f);
    }
    {
        Decoder decoder(145 * 1440, 0x12000);
        decoder.param.header = Header(2, 1, 2, 145);
        Require(sceAudiodecCpuInitDecoder(&decoder.ctrl, &decoder.memory, LpcmBd) == 0);
        Fill24(decoder.input);
        for (std::uint32_t index = 145 * 480; index < decoder.output.size(); ++index) decoder.output[index] = 1.5f;
        Require(decoder.Decode(None, None) == 0);
        Require(decoder.param.auBytes == 208800 && decoder.pcm.ui_pcm_size == 145 * 480 * 4);
        for (std::uint32_t index = 0; index < 145 * 480; ++index)
            Require(decoder.output[index] == Expected24(index % OutputSamples));
        for (std::uint32_t index = 145 * 480; index < decoder.output.size(); ++index)
            Require(decoder.output[index] == 1.5f);
    }
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

void TestInternalApiValidation() {
    Decoder decoder;
    AudiodecCpuMemory empty{0x18, nullptr, 0};
    Require(sceAudiodecCpuInternalQueryMemSize(nullptr, &decoder.memory, LpcmBd) == Error(9));
    Require(sceAudiodecCpuInternalQueryMemSize(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    AudiodecCpuMemory badSize{0x10, decoder.work.data(), 0};
    Require(sceAudiodecCpuInternalQueryMemSize(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInternalQueryMemSize(&decoder.ctrl, &empty, 0x1009) == Error(1));

    Require(sceAudiodecCpuInternalInitDecoder(nullptr, &decoder.memory, LpcmBd) == Error(9));
    Require(sceAudiodecCpuInternalInitDecoder(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    Require(sceAudiodecCpuInternalInitDecoder(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInternalInitDecoder(&decoder.ctrl, &empty, LpcmBd) == Error(0x11));
    Require(sceAudiodecCpuInternalInitDecoder(&decoder.ctrl, &decoder.memory, 0x100a) == Error(1));

    Require(sceAudiodecCpuInternalClearContext(nullptr, LpcmBd) == Error(2));
    Require(sceAudiodecCpuInternalClearContext(&empty, LpcmBd) == Error(0x11));
    Require(sceAudiodecCpuInternalClearContext(&badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInternalClearContext(&decoder.memory, 0x1009) == Error(1));

    Require(sceAudiodecCpuInternalDecode(nullptr, &decoder.memory, LpcmBd) == Error(9));
    Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, nullptr, LpcmBd) == Error(0xe));
    Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, &badSize, LpcmBd) == Error(8));
    Require(sceAudiodecCpuInternalDecode(&decoder.ctrl, &decoder.memory, 0x1009) == Error(1));
}

}

int main() {
    for (int iter = 0; iter < 2; ++iter) TestQueryMemSize();
    TestInitAndClear();
    TestDecodeArguments();
    TestDecodeSamples();
    TestDecodeWrap();
    TestTimestamps();
    TestInternalApiValidation();
    return 0;
}

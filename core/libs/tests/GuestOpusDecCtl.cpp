#include "prx/libSceOpusCeltDec/libSceOpusCeltDec.hpp"
#include "prx/libSceOpusDec/libSceOpusDec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

#include <opus.h>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

using Ctl = int (APS5_VABI*)(void*, int, ...);

struct Api {
    bool celt;
    std::uint32_t errorBase;
    int (APS5_VABI* initialize)(std::uint32_t*);
    int (APS5_VABI* terminate)(const std::uint32_t*);
    int (APS5_VABI* createEx)(const std::uint32_t*, void*, int, int);
    int (APS5_VABI* decode)(void*, const std::uint8_t*, int, std::int16_t*, int);
    Ctl ctl;
    int (APS5_VABI* destroy)(void*);
    int error(std::uint32_t offset) const { return static_cast<int>(errorBase + offset); }
};

std::vector<std::vector<std::uint8_t>> Packets(int count) {
    int error = 0;
    std::unique_ptr<OpusEncoder, void (*)(OpusEncoder*)> encoder(opus_encoder_create(48000, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error), opus_encoder_destroy);
    Require(encoder && error == OPUS_OK);
    Require(opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(24000)) == OPUS_OK);
    std::vector<std::vector<std::uint8_t>> packets;
    std::array<std::int16_t, 960 * 2> pcm{};
    for (int frame = 0; frame < count; ++frame) {
        for (int i = 0; i < 960; ++i) {
            const double t = (frame * 960 + i) / 48000.0;
            pcm[i * 2] = static_cast<std::int16_t>(9000 * std::sin(2 * 3.14159265 * 440 * t));
            pcm[i * 2 + 1] = static_cast<std::int16_t>(9000 * std::sin(2 * 3.14159265 * 440 * t + 2.5));
        }
        std::array<std::uint8_t, 1500> packet{};
        const int bytes = opus_encode(encoder.get(), pcm.data(), 960, packet.data(), packet.size());
        Require(bytes > 1 && (packet[0] >> 3) >= 16);
        packets.emplace_back(packet.begin(), packet.begin() + bytes);
    }
    return packets;
}

struct Reference {
    std::unique_ptr<OpusDecoder, void (*)(OpusDecoder*)> decoder{nullptr, opus_decoder_destroy};
    explicit Reference(int rate) {
        int error = 0;
        decoder.reset(opus_decoder_create(rate, 2, &error));
        Require(decoder && error == OPUS_OK);
    }
    std::vector<std::int16_t> Decode(const std::vector<std::uint8_t>& packet, opus_uint32* range) {
        std::vector<std::int16_t> pcm(5760 * 2);
        const int samples = opus_decode(decoder.get(), packet.data(), static_cast<int>(packet.size()), pcm.data(), 5760, 0);
        Require(samples > 0);
        pcm.resize(static_cast<std::size_t>(samples) * 2);
        Require(opus_decoder_ctl(decoder.get(), OPUS_GET_FINAL_RANGE(range)) == OPUS_OK);
        return pcm;
    }
};

std::vector<std::int16_t> Decode(const Api& api, void* state, const std::vector<std::uint8_t>& packet, int samples) {
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(samples) * 2);
    Require(api.decode(state, packet.data(), static_cast<int>(packet.size()), pcm.data(), samples * 4) == samples * 4);
    return pcm;
}

std::int32_t Get(const Api& api, void* state, int request) {
    std::int32_t value = 0x5A5A5A5A;
    Require(api.ctl(state, request, &value) == 0);
    return value;
}

void RequireRequests(const Api& api) {
    std::uint32_t context = 0;
    Require(api.initialize(&context) == 0);
    std::array<std::uint8_t, 640> state{}, unknown{};
    Require(api.createEx(&context, state.data(), 24000, 2) == 0);
    std::int32_t value = 0;
    Require(api.ctl(nullptr, OPUS_GET_GAIN_REQUEST, &value) == api.error(0));
    Require(api.ctl(unknown.data(), OPUS_GET_GAIN_REQUEST, &value) == api.error(1));
    for (int request : {0, 4011, 4027, 4030, 4032, 4048, 4052, 4100, -2})
        Require(api.ctl(state.data(), request, &value) == api.error(14));
    for (int request : {0, 4100, -2}) Require(api.ctl(state.data(), request) == api.error(14));
    if (api.celt)
        for (int request : {OPUS_SET_INBAND_FEC_REQUEST, OPUS_GET_INBAND_FEC_REQUEST, 4050, 4051})
            Require(api.ctl(state.data(), request, &value) == api.error(14));
    for (int request : {OPUS_GET_SAMPLE_RATE_REQUEST, OPUS_GET_FINAL_RANGE_REQUEST, OPUS_GET_GAIN_REQUEST, OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST})
        Require(api.ctl(state.data(), request, static_cast<std::int32_t*>(nullptr)) == api.error(13));
    if (!api.celt)
        for (int request : {OPUS_GET_INBAND_FEC_REQUEST, 4051})
            Require(api.ctl(state.data(), request, static_cast<std::int32_t*>(nullptr)) == api.error(13));
    std::int32_t first = 0, second = 0;
    Require(api.ctl(state.data(), -1, &first, static_cast<std::int32_t*>(nullptr)) == api.error(13));
    Require(api.ctl(state.data(), -1, static_cast<std::int32_t*>(nullptr), &second) == api.error(13));
    RequireThrows([&] { api.ctl(state.data(), -1, &first, &second); });

    Require(Get(api, state.data(), OPUS_GET_SAMPLE_RATE_REQUEST) == 24000);
    Require(Get(api, state.data(), OPUS_GET_FINAL_RANGE_REQUEST) == 0);
    Require(Get(api, state.data(), OPUS_GET_GAIN_REQUEST) == 0);
    for (int gain : {32768, -32769, 0x10000})
        Require(api.ctl(state.data(), OPUS_SET_GAIN_REQUEST, gain) == api.error(12));
    for (int gain : {-32768, 32767, -300})
        Require(api.ctl(state.data(), OPUS_SET_GAIN_REQUEST, gain) == 0 && Get(api, state.data(), OPUS_GET_GAIN_REQUEST) == gain);
    for (int flag : {2, -1})
        Require(api.ctl(state.data(), OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST, flag) == api.error(api.celt ? 17 : 27));
    Require(api.ctl(state.data(), OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST, 1) == 0);
    Require(Get(api, state.data(), OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST) == 1);
    if (!api.celt) {
        Require(api.ctl(state.data(), OPUS_SET_INBAND_FEC_REQUEST, 2) == api.error(16));
        Require(api.ctl(state.data(), OPUS_SET_INBAND_FEC_REQUEST, 1) == 0 && Get(api, state.data(), OPUS_GET_INBAND_FEC_REQUEST) == 1);
        Require(api.ctl(state.data(), 4050, 2) == api.error(17));
        RequireThrows([&] { api.ctl(state.data(), 4050, 1); });
        Require(api.ctl(state.data(), 4050, 0) == 0 && Get(api, state.data(), 4051) == 0);
    }
    Require(api.ctl(state.data(), OPUS_RESET_STATE) == 0);
    Require(Get(api, state.data(), OPUS_GET_GAIN_REQUEST) == 0 && Get(api, state.data(), OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST) == 0);
    if (!api.celt) Require(Get(api, state.data(), OPUS_GET_INBAND_FEC_REQUEST) == 0);
    Require(api.destroy(state.data()) == 0);
    Require(api.terminate(&context) == 0);
}

void RequireDecoding(const Api& api, const std::vector<std::vector<std::uint8_t>>& packets) {
    std::uint32_t context = 0;
    Require(api.initialize(&context) == 0);
    std::array<std::uint8_t, 640> state{};
    Require(api.createEx(&context, state.data(), 48000, 2) == 0);
    Reference reference(48000);
    opus_uint32 range = 0;
    bool gainChanged = false;
    for (std::size_t index = 0; index < 4; ++index) {
        const auto expected = reference.Decode(packets[index], &range);
        const auto plain = Decode(api, state.data(), packets[index], 960);
        Require(plain == expected && static_cast<opus_uint32>(Get(api, state.data(), OPUS_GET_FINAL_RANGE_REQUEST)) == range);
    }
    Require(api.ctl(state.data(), OPUS_SET_GAIN_REQUEST, -1536) == 0);
    Require(api.ctl(state.data(), OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST, 1) == 0);
    Require(opus_decoder_ctl(reference.decoder.get(), OPUS_SET_GAIN(-1536)) == OPUS_OK);
    Require(opus_decoder_ctl(reference.decoder.get(), OPUS_SET_PHASE_INVERSION_DISABLED(1)) == OPUS_OK);
    Reference unscaled(48000);
    opus_uint32 unscaledRange = 0;
    for (std::size_t index = 0; index < 4; ++index) unscaled.Decode(packets[index], &unscaledRange);
    for (std::size_t index = 4; index < 8; ++index) {
        const auto expected = reference.Decode(packets[index], &range);
        const auto scaled = Decode(api, state.data(), packets[index], 960);
        Require(scaled == expected && static_cast<opus_uint32>(Get(api, state.data(), OPUS_GET_FINAL_RANGE_REQUEST)) == range);
        gainChanged = gainChanged || scaled != unscaled.Decode(packets[index], &unscaledRange);
    }
    Require(gainChanged);
    Require(api.ctl(state.data(), OPUS_RESET_STATE) == 0);
    Require(static_cast<opus_uint32>(Get(api, state.data(), OPUS_GET_FINAL_RANGE_REQUEST)) == range);
    Reference fresh(48000);
    for (std::size_t index = 8; index < packets.size(); ++index) {
        const auto expected = fresh.Decode(packets[index], &range);
        Require(Decode(api, state.data(), packets[index], 960) == expected);
    }
    Require(api.destroy(state.data()) == 0);
    Require(api.terminate(&context) == 0);
}

} // namespace

int main() {
    const Api opus{false, 0x81740000u, sceOpusDecInitialize, sceOpusDecTerminate, sceOpusDecCreateEx, sceOpusDecDecode, sceOpusDecCtl, sceOpusDecDestroy};
    const Api celt{true, 0x81400000u, sceOpusCeltDecInitialize, sceOpusCeltDecTerminate, sceOpusCeltDecCreateEx, sceOpusCeltDecDecode, sceOpusCeltDecCtl, sceOpusCeltDecDestroy};
    const auto packets = Packets(12);
    for (const auto* api : {&opus, &celt}) {
        RequireRequests(*api);
        RequireDecoding(*api, packets);
    }
}

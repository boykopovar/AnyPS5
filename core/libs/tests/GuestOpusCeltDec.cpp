#include "prx/libc/include/general/VabiMacros.hpp"
#include "OpusCeltTestData.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t*);
int APS5_VABI sceOpusCeltDecTerminate(std::uint32_t*);
int APS5_VABI sceOpusCeltDecGetSize(int);
int APS5_VABI sceOpusCeltDecCreateEx(std::uint32_t*, void*, int, int);
int APS5_VABI sceOpusCeltDecDecode(void*, const std::uint8_t*, int, std::int16_t*, int);
int APS5_VABI sceOpusCeltDecDestroy(void*);
}

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Opus regression failed at line %u\n", location.line());
        std::abort();
    }
}

template<class T> void Throws(T action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    Require(threw);
}

struct Api {
    decltype(&sceOpusCeltDecInitialize) Initialize;
    decltype(&sceOpusCeltDecTerminate) Terminate;
    decltype(&sceOpusCeltDecGetSize) GetSize;
    decltype(&sceOpusCeltDecCreateEx) Create;
    decltype(&sceOpusCeltDecDecode) Decode;
    decltype(&sceOpusCeltDecDestroy) Destroy;
};

constexpr Api Celt{sceOpusCeltDecInitialize, sceOpusCeltDecTerminate, sceOpusCeltDecGetSize, sceOpusCeltDecCreateEx, sceOpusCeltDecDecode, sceOpusCeltDecDestroy};

template<std::size_t N, std::size_t P>
void CheckPacket(const Api& api, void* state, const std::uint8_t (&packet)[P], const std::int16_t (&reference)[N]) {
    std::array<std::int16_t, N + 2> output;
    output.fill(12345);
    Throws([&] { api.Decode(state, packet, P, output.data() + 1, N * 2 - 1); });
    Require(std::all_of(output.begin(), output.end(), [](auto x) { return x == 12345; }));
    Require(api.Decode(state, packet, P, output.data() + 1, N * 2) == N * 2);
    Require(output.front() == 12345 && output.back() == 12345);
    int maxError = 0;
    for (std::size_t i = 0; i < N; ++i) maxError = std::max(maxError, std::abs(int(output[i + 1]) - reference[i]));
    if (maxError > 12) std::fprintf(stderr, "Opus reference max error: %d\n", maxError);
    Require(maxError <= 12);
}

void Test(const Api& api, int channels) {
    std::uint32_t context = 0;
    Require(api.Initialize(&context) == 0 && context != 0);
    const int size = api.GetSize(channels);
    Require(size > 0);
    std::vector<std::uint8_t> storage(size + 64, 0xA5);
    void* state = storage.data() + 32;
    Require(api.Create(&context, state, 48000, channels) == 0);
    Throws([&] { api.Create(&context, state, 48000, channels); });
    Throws([&] { api.Terminate(&context); });
    if (channels == 1) {
        CheckPacket(api, state, MonoPacket0, MonoPcm0);
        CheckPacket(api, state, MonoPacket1, MonoPcm1);
    } else {
        CheckPacket(api, state, StereoPacket0, StereoPcm0);
        CheckPacket(api, state, StereoPacket1, StereoPcm1);
    }
    Require(std::all_of(storage.begin(), storage.begin() + 32, [](auto x) { return x == 0xA5; }));
    Require(std::all_of(storage.end() - 32, storage.end(), [](auto x) { return x == 0xA5; }));
    Require(api.Destroy(state) == 0);
    Throws([&] { api.Destroy(state); });
    std::int16_t output[240]{};
    Throws([&] { api.Decode(state, MonoPacket0, sizeof(MonoPacket0), output, sizeof(output)); });
    Require(api.Create(&context, state, 48000, channels) == 0);
    if (channels == 1) CheckPacket(api, state, MonoPacket0, MonoPcm0);
    else CheckPacket(api, state, StereoPacket0, StereoPcm0);
    Require(api.Destroy(state) == 0);
    Require(api.Terminate(&context) == 0 && context == 0);
    Throws([&] { api.Terminate(&context); });
}

void Invalid(const Api& api) {
    Throws([&] { api.Initialize(nullptr); });
    Throws([&] { api.Terminate(nullptr); });
    Throws([&] { api.GetSize(0); });
    Throws([&] { api.GetSize(3); });
    std::uint32_t context = 0;
    std::vector<std::uint8_t> state(api.GetSize(1));
    std::array<std::int16_t, 240> output;
    output.fill(9876);
    Throws([&] { api.Create(&context, state.data(), 48000, 1); });
    Require(api.Initialize(&context) == 0);
    Throws([&] { api.Create(nullptr, state.data(), 48000, 1); });
    Throws([&] { api.Create(&context, nullptr, 48000, 1); });
    Throws([&] { api.Create(&context, state.data(), 24000, 1); });
    Require(api.Create(&context, state.data(), 48000, 1) == 0);
    Throws([&] { api.Decode(state.data(), nullptr, 3, output.data(), sizeof(output)); });
    Throws([&] { api.Decode(state.data(), MonoPacket0, 0, output.data(), sizeof(output)); });
    Throws([&] { api.Decode(state.data(), MonoPacket0, sizeof(MonoPacket0), nullptr, sizeof(output)); });
    const std::uint8_t truncated[] = {0x83};
    const std::uint8_t tooLong[] = {0x83, 63};
    Throws([&] { api.Decode(state.data(), truncated, sizeof(truncated), output.data(), sizeof(output)); });
    Throws([&] { api.Decode(state.data(), tooLong, sizeof(tooLong), output.data(), sizeof(output)); });
    Throws([&] { api.Decode(state.data(), SilkPacket0, sizeof(SilkPacket0), output.data(), sizeof(output)); });
    Require(std::all_of(output.begin(), output.end(), [](auto x) { return x == 9876; }));
    CheckPacket(api, state.data(), MonoPacket0, MonoPcm0);
    Require(api.Destroy(state.data()) == 0);
    Require(api.Terminate(&context) == 0);
}

void LongPacket(const Api& api) {
    std::uint32_t context = 0;
    Require(api.Initialize(&context) == 0);
    std::vector<std::uint8_t> state(api.GetSize(2));
    Require(api.Create(&context, state.data(), 48000, 2) == 0);
    std::array<std::uint8_t, 98> packet;
    packet[0] = 0x83;
    packet[1] = 48;
    for (std::size_t i = 2; i < packet.size(); i += 2) {
        packet[i] = 0xFF;
        packet[i + 1] = 0xFE;
    }
    std::array<std::int16_t, 5760 * 2 + 2> output;
    output.fill(9876);
    Require(api.Decode(state.data(), packet.data(), packet.size(), output.data() + 1, 5760 * 4) == 5760 * 4);
    Require(output.front() == 9876 && output.back() == 9876);
    Require(std::all_of(output.begin() + 1, output.end() - 1, [](auto x) { return std::abs(int(x)) <= 1; }));
    Require(api.Destroy(state.data()) == 0);
    Require(api.Terminate(&context) == 0);
}

void Isolation() {
    std::uint32_t first = 0, second = 0;
    Require(Celt.Initialize(&first) == 0);
    Require(Celt.Initialize(&second) == 0);
    std::vector<std::uint8_t> a(Celt.GetSize(1)), b(Celt.GetSize(1));
    Require(Celt.Create(&first, a.data(), 48000, 1) == 0);
    Require(Celt.Create(&second, b.data(), 48000, 1) == 0);
    CheckPacket(Celt, a.data(), MonoPacket0, MonoPcm0);
    CheckPacket(Celt, b.data(), MonoPacket0, MonoPcm0);
    Require(Celt.Destroy(a.data()) == 0);
    Require(Celt.Destroy(b.data()) == 0);
    Require(Celt.Terminate(&first) == 0);
    Require(Celt.Terminate(&second) == 0);
}

int main() {
    Test(Celt, 1);
    Test(Celt, 2);
    Invalid(Celt);
    LongPacket(Celt);
    Isolation();
}

#include "prx/libSceAudioOut/include/HostPause.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "audio out host pause: %s\n", reason);
        std::abort();
    }
}

void FlagDefault() {
    Require(!AudioOutHostPaused_nid_no_patch(), "the flag starts set");
    AudioOutSetHostPause_nid_no_patch(true);
    Require(AudioOutHostPaused_nid_no_patch(), "the flag does not set");
    AudioOutSetHostPause_nid_no_patch(true);
    Require(AudioOutHostPaused_nid_no_patch(), "setting twice cleared the flag");
    AudioOutSetHostPause_nid_no_patch(false);
    Require(!AudioOutHostPaused_nid_no_patch(), "the flag does not clear");
}

void BufferPassThroughWhenClear() {
    std::vector<std::uint8_t> samples(64);
    for (std::size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<std::uint8_t>(i + 1);
    std::vector<std::uint8_t> silence;
    Require(AudioOutSilenceWhilePaused(samples.data(), 64, silence) == samples.data(), "an unpaused buffer is not passed through by pointer");
    Require(silence.empty(), "an unpaused buffer allocated silence");
}

void BufferSilentWhenPaused() {
    std::vector<std::uint8_t> samples(64, 0x55);
    std::vector<std::uint8_t> silence(7, 0x77);
    AudioOutSetHostPause_nid_no_patch(true);
    const void* out = AudioOutSilenceWhilePaused(samples.data(), 64, silence);
    AudioOutSetHostPause_nid_no_patch(false);
    Require(out == silence.data() && out != samples.data(), "a paused buffer is not replaced");
    Require(silence.size() == 64 && std::all_of(silence.begin(), silence.end(), [](std::uint8_t b) { return b == 0; }), "the replacement is not 64 zero bytes");
    Require(std::all_of(samples.begin(), samples.end(), [](std::uint8_t b) { return b == 0x55; }), "the title's buffer was written");
}

void FloatsSilentWhenPaused() {
    std::vector<float> samples(32, 0.5f);
    AudioOutSilenceFloatsWhilePaused(samples.data(), samples.size());
    Require(std::all_of(samples.begin(), samples.end(), [](float v) { return v == 0.5f; }), "unpaused floats were changed");
    AudioOutSetHostPause_nid_no_patch(true);
    AudioOutSilenceFloatsWhilePaused(samples.data(), 16);
    AudioOutSetHostPause_nid_no_patch(false);
    Require(std::all_of(samples.begin(), samples.begin() + 16, [](float v) { return v == 0.0f; }), "paused floats were not zeroed");
    Require(std::all_of(samples.begin() + 16, samples.end(), [](float v) { return v == 0.5f; }), "floats past the count were zeroed");
}

void ConcurrentReaders() {
    std::atomic<bool> stop{false};
    std::atomic<int> seen{0};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                if (AudioOutHostPaused_nid_no_patch()) seen.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }
    for (int i = 0; i < 200; ++i) {
        AudioOutSetHostPause_nid_no_patch(i % 2 == 0);
        std::this_thread::yield();
    }
    AudioOutSetHostPause_nid_no_patch(true);
    for (int i = 0; i < 2000 && seen.load() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    stop.store(true);
    for (auto& reader : readers) reader.join();
    AudioOutSetHostPause_nid_no_patch(false);
    Require(seen.load() > 0, "no reader saw the flag set from another thread");
}

}

int main() {
    FlagDefault();
    BufferPassThroughWhenClear();
    BufferSilentWhenPaused();
    FloatsSilentWhenPaused();
    ConcurrentReaders();
    std::puts("audio out host pause: all checks passed");
    return 0;
}

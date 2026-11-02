#include "prx/libScePad/include/PadAudioHapticQueue.hpp"
#include <cstdio>

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<typename TAction>
static bool Throws(TAction action) {
    try { action(); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}

int main() {
    try {
        {
            PadAudioHapticQueue queue;
            queue.Queue(1, 1000, 6333, 128, 255);
            Require(queue.Read(999) == std::array<std::uint8_t, 2>{}, "a queued block began early");
            Require(queue.Read(1000) == std::array<std::uint8_t, 2>{128, 255}, "a block did not begin at its queued start");
            Require(queue.Read(6332) == std::array<std::uint8_t, 2>{128, 255}, "a block ended early");
            Require(queue.Read(6333) == std::array<std::uint8_t, 2>{}, "a short block inherited an 80 ms hold");
        }
        {
            PadAudioHapticQueue queue;
            queue.Queue(1, 1000, 201000, 200, 100);
            Require(queue.Read(81000) == std::array<std::uint8_t, 2>{200, 100}, "a long block expired after 80 ms");
            Require(queue.Read(200999) == std::array<std::uint8_t, 2>{200, 100}, "a long block ended before its frame duration");
            Require(queue.Read(201000) == std::array<std::uint8_t, 2>{}, "a long block outlived its frame duration");
        }
        {
            PadAudioHapticQueue queue;
            queue.Queue(1, 100, 200, 200, 10);
            queue.Queue(1, 200, 300, 0, 0);
            queue.Queue(1, 400, 500, 20, 30);
            Require(queue.Read(199) == std::array<std::uint8_t, 2>{200, 10}, "the first grain changed");
            Require(queue.Read(200) == std::array<std::uint8_t, 2>{}, "silent PCM retained an earlier peak");
            Require(queue.Read(350) == std::array<std::uint8_t, 2>{}, "a gap between blocks vibrated");
            Require(queue.Read(400) == std::array<std::uint8_t, 2>{20, 30}, "a later grain did not start");
        }
        {
            PadAudioHapticQueue queue;
            queue.Queue(1, 0, 1000, 200, 10);
            queue.Queue(2, 0, 1000, 20, 220);
            Require(queue.Read(10) == std::array<std::uint8_t, 2>{200, 220}, "independent sources did not combine");
            queue.Clear(1);
            Require(queue.Read(10) == std::array<std::uint8_t, 2>{20, 220}, "draining one source cleared another");
            queue.Clear(1);
            queue.Clear(99);
            Require(queue.Read(10) == std::array<std::uint8_t, 2>{20, 220}, "clearing an absent source changed output");
            queue.Clear(2);
            Require(queue.Read(10) == std::array<std::uint8_t, 2>{}, "closing the final source retained output");
        }
        {
            PadAudioHapticQueue queue;
            Require(Throws([&] { queue.Queue(0, 0, 1, 1, 1); }), "an invalid source was accepted");
            Require(Throws([&] { queue.Queue(1, 1, 1, 1, 1); }), "a zero-length interval was accepted");
            Require(Throws([&] { queue.Queue(1, 2, 1, 1, 1); }), "a reversed interval was accepted");
            queue.Queue(1, 0, 100, 10, 20);
            Require(Throws([&] { queue.Queue(1, 99, 200, 1, 1); }), "overlapping source blocks were accepted");
            Require(queue.Read(50) == std::array<std::uint8_t, 2>{10, 20}, "a refused interval changed the active block");
        }
        std::puts("legacy vibration timeline tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

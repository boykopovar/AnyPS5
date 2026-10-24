#include "prx/libSceVideoOut/include/PadOutputPacing.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace std::chrono_literals;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "pad_output_pacing: %s\n", message);
    std::exit(1);
}

void LightBarSendsChangesOnly() {
    LightBarPacer pacer;
    const auto start = LightBarPacer::Clock::time_point{} + 1s;
    const LightBarPacer::Color blue{0, 64, 255};
    Require(pacer.Due(blue, start), "the first colour is sent at once");
    pacer.Result(blue, start, true);
    Require(!pacer.Pending(blue), "a colour that was sent is not sent again");
    Require(!pacer.Due(blue, start + 1s), "an unchanged colour stays quiet");
    const LightBarPacer::Color red{255, 0, 0};
    Require(pacer.Pending(red) && !pacer.Due(red, start + 10ms), "a new colour waits for the interval");
    Require(pacer.Due(red, start + LightBarPacer::Interval), "a new colour goes out once the interval has passed");
}

void LightBarRetriesFailures() {
    LightBarPacer pacer;
    const auto start = LightBarPacer::Clock::time_point{} + 1s;
    const LightBarPacer::Color green{0, 255, 0};
    pacer.Result(green, start, false);
    Require(pacer.Pending(green), "a colour SDL refused is still pending");
    Require(!pacer.Due(green, start + 1ms), "a refused colour is not retried on the next frame");
    Require(pacer.Due(green, start + LightBarPacer::Interval), "a refused colour is retried after the interval");
    pacer.Result(green, start + LightBarPacer::Interval, true);
    Require(!pacer.Pending(green), "a retried colour that was sent is done");
    pacer.Reset();
    Require(pacer.Due(green, start + LightBarPacer::Interval), "a new controller gets the current colour");
}

void LightBarAnimationIsPaced() {
    LightBarPacer pacer;
    const auto start = LightBarPacer::Clock::time_point{} + 1s;
    constexpr int frames = 120;
    int sent = 0;
    LightBarPacer::Color color{};
    for (int frame = 0; frame < frames; ++frame) {
        const auto now = start + std::chrono::microseconds(frame * 8333);
        color = {static_cast<std::uint8_t>(frame), 32, 32};
        if (!pacer.Due(color, now)) continue;
        pacer.Result(color, now, true);
        ++sent;
    }
    Require(sent >= 30 && sent <= 34, "a colour that changes every frame at 120 Hz goes out about 33 times a second");
    const auto end = start + 1s;
    Require(pacer.Pending(color), "the last colour of the animation is still owed");
    Require(pacer.Due(color, end + LightBarPacer::Interval), "the last colour of the animation is sent when the interval allows");
}

void TriggerEffectRetriesFailures() {
    TriggerEffectCache cache;
    TriggerEffectCache::Effect effect{};
    effect[0] = 0x0C;
    effect[10] = 0x21;
    Require(cache.NeedsSend(effect), "the first effect is sent");
    cache.Result(effect, false);
    Require(cache.NeedsSend(effect), "an effect SDL refused is sent again");
    cache.Result(effect, true);
    Require(!cache.NeedsSend(effect), "an effect that was sent is not repeated");
    auto changed = effect;
    changed[21] = 0x26;
    Require(cache.NeedsSend(changed), "a changed effect is sent");
    cache.Reset();
    Require(cache.NeedsSend(effect), "a new controller gets the current effect");
}

}

int main() {
    LightBarSendsChangesOnly();
    LightBarRetriesFailures();
    LightBarAnimationIsPaced();
    TriggerEffectRetriesFailures();
    std::puts("pad_output_pacing: ok");
    return 0;
}

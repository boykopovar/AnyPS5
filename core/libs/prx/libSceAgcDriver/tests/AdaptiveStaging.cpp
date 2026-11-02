#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::AdaptiveStaging;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint64_t Begin = 0x200000000;
constexpr std::uint64_t End = Begin + 0xca80000;
constexpr std::int64_t FrameMs = 100;

struct Title {
    AdaptiveStaging policy;
    std::vector<std::string> lines;
    std::uint64_t frames = 0;
    double busyMs = 0;
    std::int64_t nowMs = 1;

    Title() {
        policy.report = [this](const std::string& line) { lines.push_back(line); };
    }

    std::uint64_t Run(std::uint64_t count, const std::function<double(bool, std::int64_t)>& cost) {
        std::uint64_t inPlace = 0;
        for (std::uint64_t frame = 0; frame < count; ++frame) {
            const bool kept = policy.InPlace(Begin, End, true, frames, busyMs, nowMs);
            inPlace += kept ? 1 : 0;
            busyMs += cost(kept, nowMs);
            ++frames;
            nowMs += FrameMs;
        }
        return inPlace;
    }

    bool InPlaceNow() {
        return policy.InPlace(Begin, End, false, frames, busyMs, nowMs);
    }

    std::uint64_t Count(const std::string& text) const {
        std::uint64_t count = 0;
        for (const auto& line : lines) count += line.find(text) != std::string::npos ? 1 : 0;
        return count;
    }
};

void CheckFasterInPlaceIsKept() {
    Title title;
    title.Run(200, [](bool inPlace, std::int64_t) { return inPlace ? 73.0 : 91.0; });
    Require(title.InPlaceNow(), "an element 18 ms faster per frame in place is still staged");
    Require(title.Count("-> in place") >= 1, "no decision line chose in place");
}

void CheckFasterStagedStaysStaged() {
    Title title;
    title.Run(200, [](bool inPlace, std::int64_t) { return inPlace ? 114.0 : 84.0; });
    Require(!title.InPlaceNow(), "an element 30 ms faster per frame staged was moved in place");
    Require(title.Count("-> staged") >= 1, "no decision line kept it staged");
}

void CheckNoiseKeepsTheMode() {
    Title title;
    std::uint64_t frame = 0;
    const auto inPlace = title.Run(3000, [&](bool, std::int64_t) { return 80.0 + static_cast<double>((frame++ * 7) % 11) - 5.0; });
    Require(title.Count("-> in place") == 0, "frame noise alone moved the element in place");
    Require(inPlace < 3000 / 4, "frame noise kept the element in place for " + std::to_string(inPlace) + " of 3000 frames");
}

void CheckBackOffGrowsToTheCap() {
    Title title;
    title.Run(6000, [](bool inPlace, std::int64_t) { return inPlace ? 114.0 : 84.0; });
    Require(title.Count("next probe in 40 s") >= 1, "the second repeated answer did not wait 40 s");
    Require(title.Count("next probe in 160 s") >= 1, "the back-off never reached 160 s");
    Require(title.Count("next probe in 320 s") == 0, "the back-off grew past 160 s");
}

void CheckSceneChangeProbesAgain() {
    Title title;
    const std::int64_t changeMs = 60000;
    const auto cost = [&](bool inPlace, std::int64_t nowMs) {
        if (nowMs < changeMs) return inPlace ? 73.0 : 91.0;
        return inPlace ? 300.0 : 200.0;
    };
    title.Run(600, cost);
    Require(title.InPlaceNow(), "the first scene did not keep the element in place");
    title.Run(400, cost);
    Require(title.Count("probing again") >= 1, "a scene change from 73 to about 300 ms per frame did not probe again");
    Require(!title.InPlaceNow(), "after the scene change the element is still in place although staged is faster");
}

void CheckSmallElementIsNotProbed() {
    AdaptiveStaging policy;
    std::uint64_t frames = 0;
    double busyMs = 0;
    std::int64_t nowMs = 1;
    constexpr std::uint64_t SmallBegin = 0x100000000;
    constexpr std::uint64_t SmallEnd = SmallBegin + 0x10000;
    bool smallInPlace = false;
    for (int frame = 0; frame < 2000; ++frame) {
        const bool bigInPlace = policy.InPlace(Begin, End, true, frames, busyMs, nowMs);
        smallInPlace = smallInPlace || policy.InPlace(SmallBegin, SmallEnd, true, frames, busyMs, nowMs);
        busyMs += bigInPlace ? 73.0 : 91.0;
        ++frames;
        nowMs += FrameMs;
    }
    Require(!smallInPlace, "a 64 KiB element next to a 202 MiB one was probed in place");
}

}

int main() {
    try {
        CheckFasterInPlaceIsKept();
        CheckFasterStagedStaysStaged();
        CheckNoiseKeepsTheMode();
        CheckBackOffGrowsToTheCap();
        CheckSceneChangeProbesAgain();
        CheckSmallElementIsNotProbed();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "adaptive staging tests passed\n";
    return 0;
}

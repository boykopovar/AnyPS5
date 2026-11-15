#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ADAPTIVESTAGING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_ADAPTIVESTAGING_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class AdaptiveStaging {
public:
    static constexpr std::uint64_t SettleFrames = 4;
    static constexpr std::uint64_t MeasuredFrames = 8;
    static constexpr std::uint64_t EarlyFrames = 3;
    static constexpr std::int64_t ReprobeMs = 20000;
    static constexpr std::int64_t MaxReprobeMs = 160000;
    static constexpr std::int64_t ShareWindowMs = 10000;
    static constexpr double SceneShift = 0.25;

    std::function<void(const std::string&)> report;

    bool InPlace(std::uint64_t begin, std::uint64_t end, bool use, std::uint64_t frames, double busyMs, std::int64_t nowMs) {
        const Key key{begin, end};
        auto& element = elements[key];
        if (use) element.bytes += end - begin;
        tick(frames, busyMs, nowMs);
        if (probe.active && probe.key == key) return probe.phase == 1 ? !element.inPlace : element.inPlace;
        return element.inPlace;
    }

private:
    using Key = std::pair<std::uint64_t, std::uint64_t>;

    struct Element {
        std::uint64_t bytes = 0;
        bool inPlace = false;
        std::int64_t nextProbeMs = 0;
        std::int64_t intervalMs = ReprobeMs;
    };
    struct Probe {
        bool active = false;
        Key key{};
        int phase = 0;
        std::uint64_t phaseFrames = 0;
        std::vector<double> samples[3];
    };

    void say(const char* format, auto... args) {
        if (!report) return;
        char text[256];
        std::snprintf(text, sizeof(text), format, args...);
        report(text);
    }

    void tick(std::uint64_t frames, double busyMs, std::int64_t now) {
        if (frames == lastFrames) return;
        const auto newFrames = frames - lastFrames;
        const double perFrame = (busyMs - lastBusyMs) / static_cast<double>(newFrames);
        lastFrames = frames;
        lastBusyMs = busyMs;
        if (now - windowStartMs >= ShareWindowMs) {
            for (auto& [key, element] : elements) element.bytes /= 2;
            windowStartMs = now;
        }
        if (!probe.active) {
            watchScene(perFrame, newFrames, now);
            start(now);
            return;
        }
        auto& element = elements[probe.key];
        probe.phaseFrames += newFrames;
        if (probe.phaseFrames <= SettleFrames) return;
        if (perFrame > 0) probe.samples[probe.phase].push_back(perFrame);
        if (probe.phase == 1 && clearlySlower()) {
            decide(element, now);
            return;
        }
        if (probe.phaseFrames < SettleFrames + MeasuredFrames) return;
        if (++probe.phase < 3) {
            probe.phaseFrames = 0;
            return;
        }
        decide(element, now);
    }

    bool clearlySlower() const {
        const auto& current = probe.samples[0];
        const auto& other = probe.samples[1];
        if (current.size() < MeasuredFrames || other.size() < EarlyFrames) return false;
        return *std::min_element(other.begin(), other.end()) > *std::max_element(current.begin(), current.end());
    }

    void watchScene(double perFrame, std::uint64_t newFrames, std::int64_t now) {
        if (perFrame <= 0 || decidedMs <= 0) return;
        if (settling > 0) {
            settling -= std::min(settling, newFrames);
            return;
        }
        sceneMs = sceneMs * 0.9 + perFrame * 0.1;
        if (std::abs(sceneMs - decidedMs) <= SceneShift * decidedMs) return;
        say("[staging] adaptive: frame GPU %.1f -> %.1f ms, probing again", decidedMs, sceneMs);
        for (auto& [key, element] : elements) {
            element.nextProbeMs = std::min(element.nextProbeMs, now);
            element.intervalMs = ReprobeMs;
        }
        decidedMs = 0;
    }

    void start(std::int64_t now) {
        std::uint64_t total = 0;
        for (const auto& [key, element] : elements) total += element.bytes;
        const Element* best = nullptr;
        Key bestKey{};
        for (const auto& [key, element] : elements) {
            if (element.nextProbeMs > now || element.bytes * 10 < total) continue;
            if (best == nullptr || element.bytes > best->bytes) {
                best = &element;
                bestKey = key;
            }
        }
        if (best == nullptr) return;
        probe = Probe{};
        probe.active = true;
        probe.key = bestKey;
    }

    void decide(Element& element, std::int64_t now) {
        std::vector<double> current = probe.samples[0];
        current.insert(current.end(), probe.samples[2].begin(), probe.samples[2].end());
        const auto& other = probe.samples[1];
        const auto stats = [](const std::vector<double>& values) {
            double sum = 0, squares = 0;
            for (const auto value : values) sum += value;
            const double mean = values.empty() ? 0 : sum / static_cast<double>(values.size());
            for (const auto value : values) squares += (value - mean) * (value - mean);
            const double variance = values.size() > 1 ? squares / static_cast<double>(values.size() - 1) : 0;
            return std::pair{mean, values.empty() ? 0.0 : variance / static_cast<double>(values.size())};
        };
        const auto [currentMean, currentError] = stats(current);
        const auto [otherMean, otherError] = stats(other);
        const bool measured = current.size() >= MeasuredFrames && other.size() >= EarlyFrames;
        const bool flip = measured && otherMean < currentMean - 2.0 * std::sqrt(currentError + otherError);
        const double stagedMs = element.inPlace ? otherMean : currentMean;
        const double inPlaceMs = element.inPlace ? currentMean : otherMean;
        if (flip) element.inPlace = !element.inPlace;
        element.intervalMs = flip || !measured ? ReprobeMs : std::min(element.intervalMs * 2, MaxReprobeMs);
        element.nextProbeMs = now + element.intervalMs;
        decidedMs = flip ? otherMean : currentMean;
        sceneMs = decidedMs;
        settling = SettleFrames;
        say("[staging] adaptive: 0x%llx+0x%llx GPU %.1f ms/frame staged, %.1f in place (%zu/%zu frames) -> %s%s, next probe in %lld s", static_cast<unsigned long long>(probe.key.first), static_cast<unsigned long long>(probe.key.second - probe.key.first), stagedMs, inPlaceMs, current.size(), other.size(), element.inPlace ? "in place" : "staged", measured ? "" : " (not measured)", static_cast<long long>(element.intervalMs / 1000));
        probe = Probe{};
    }

    std::map<Key, Element> elements;
    Probe probe;
    std::uint64_t lastFrames = 0;
    double lastBusyMs = 0;
    double decidedMs = 0;
    double sceneMs = 0;
    std::uint64_t settling = 0;
    std::int64_t windowStartMs = 0;
};

}

#endif

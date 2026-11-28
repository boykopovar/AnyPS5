#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADRECORDING_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADRECORDING_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Pad {

struct RecordedState {
    std::uint32_t buttons = 0;
    std::array<std::uint8_t, 4> sticks{128, 128, 128, 128};
    std::array<std::uint8_t, 2> triggers{};

    bool operator==(const RecordedState&) const = default;
};

struct RecordedSample {
    std::uint64_t ms = 0;
    RecordedState state;
};

inline constexpr std::string_view RecordingHeader = "APS5_PAD_RECORDING 1";

inline std::string FormatRecordedSample(const RecordedSample& sample) {
    char line[96];
    std::snprintf(line, sizeof(line), "%llu %x %u %u %u %u %u %u\n", static_cast<unsigned long long>(sample.ms), sample.state.buttons,
        sample.state.sticks[0], sample.state.sticks[1], sample.state.sticks[2], sample.state.sticks[3], sample.state.triggers[0], sample.state.triggers[1]);
    return line;
}

inline std::optional<std::vector<RecordedSample>> ParseRecording(std::string_view text) {
    std::istringstream lines{std::string(text)};
    std::string line;
    if (!std::getline(lines, line)) return std::nullopt;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != RecordingHeader) return std::nullopt;
    std::vector<RecordedSample> samples;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::array<std::uint64_t, 8> fields{};
        const char* cursor = line.data();
        const char* end = line.data() + line.size();
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i != 0 && (cursor == end || *cursor != ' ')) return std::nullopt;
            while (cursor < end && *cursor == ' ') ++cursor;
            const auto [next, error] = std::from_chars(cursor, end, fields[i], i == 1 ? 16 : 10);
            if (error != std::errc{}) return std::nullopt;
            cursor = next;
        }
        if (cursor != end || fields[1] > 0xffffffffu) return std::nullopt;
        if (std::any_of(fields.begin() + 2, fields.end(), [](std::uint64_t value) { return value > 255u; })) return std::nullopt;
        if (!samples.empty() && fields[0] < samples.back().ms) return std::nullopt;
        RecordedSample sample;
        sample.ms = fields[0];
        sample.state.buttons = static_cast<std::uint32_t>(fields[1]);
        for (std::size_t i = 0; i < 4; ++i) sample.state.sticks[i] = static_cast<std::uint8_t>(fields[2 + i]);
        for (std::size_t i = 0; i < 2; ++i) sample.state.triggers[i] = static_cast<std::uint8_t>(fields[6 + i]);
        samples.push_back(sample);
    }
    return samples;
}

class RecordingWriter {
public:
    explicit RecordingWriter(const std::filesystem::path& path) : filePath(path) {
        if (path.empty()) return;
        file.open(path, std::ios::binary | std::ios::trunc);
        if (file) file << RecordingHeader << '\n' << std::flush;
        if (!file) throw std::runtime_error("APS5_PAD_RECORD: cannot write '" + path.string() + "'");
    }

    bool Active() const { return file.is_open(); }

    void Record(std::uint64_t nowUs, const RecordedState& state) {
        if (!Active()) return;
        if (!started) {
            started = true;
            startUs = nowUs;
        } else if (state == last) {
            return;
        }
        last = state;
        file << FormatRecordedSample({(nowUs - startUs) / 1000u, state}) << std::flush;
        if (!file) throw std::runtime_error("APS5_PAD_RECORD: cannot write '" + filePath.string() + "'");
    }

private:
    std::filesystem::path filePath;
    std::ofstream file;
    bool started = false;
    std::uint64_t startUs = 0;
    RecordedState last;
};

class RecordingPlayer {
public:
    explicit RecordingPlayer(const std::filesystem::path& path) {
        if (path.empty()) return;
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("APS5_PAD_REPLAY: cannot read '" + path.string() + "'");
        std::ostringstream text;
        text << input.rdbuf();
        auto parsed = ParseRecording(text.str());
        if (!parsed) {
            throw std::runtime_error("APS5_PAD_REPLAY: '" + path.string() + "' is not a recording: it needs the line '" + std::string(RecordingHeader) +
                "', then one line '<ms> <buttons hex> <lx> <ly> <rx> <ry> <l2> <r2>' per change in time order");
        }
        samples = std::move(*parsed);
        active = true;
    }

    explicit RecordingPlayer(std::vector<RecordedSample> samples) : samples(std::move(samples)), active(true) {}

    bool Active() const { return active; }

    std::optional<RecordedState> At(std::uint64_t nowUs) {
        if (!active) return std::nullopt;
        if (!started) {
            started = true;
            startUs = nowUs;
        }
        const auto elapsed = (nowUs - startUs) / 1000u;
        const auto next = std::upper_bound(samples.begin(), samples.end(), elapsed,
            [](std::uint64_t time, const RecordedSample& sample) { return time < sample.ms; });
        if (next == samples.begin()) return RecordedState{};
        return std::prev(next)->state;
    }

private:
    std::vector<RecordedSample> samples;
    bool active = false;
    bool started = false;
    std::uint64_t startUs = 0;
};

}

#endif

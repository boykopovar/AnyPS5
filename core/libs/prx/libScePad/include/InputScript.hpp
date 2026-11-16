#ifndef CORE_LIBS_PRX_LIBSCEPAD_INPUTSCRIPT_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INPUTSCRIPT_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Pad {
class InputScript {
public:
    static constexpr std::uint64_t kDefaultHoldMs = 150;
    static constexpr std::uint64_t kMaxTimeMs = 86400000;
    static constexpr std::size_t kMaxEntries = 20000;
    static constexpr std::uint64_t kLivePollUs = 250000;

    struct Entry {
        std::uint64_t startMs = 0;
        std::uint64_t holdMs = kDefaultHoldMs;
        std::uint32_t buttons = 0;
        int stick = -1;
        std::array<std::uint8_t, 2> axes{128, 128};
        std::string label;
    };

    struct Fired {
        std::uint64_t elapsedMs = 0;
        std::uint64_t holdMs = 0;
        std::string label;
    };

    struct Frame {
        std::uint32_t buttons = 0;
        bool hasSticks = false;
        std::array<std::uint8_t, 4> sticks{128, 128, 128, 128};
        std::uint8_t analogL2 = 0;
        std::uint8_t analogR2 = 0;
    };

    struct Report {
        std::vector<Fired> fired;
        std::vector<std::string> errors;
    };

    static std::vector<Entry> ParseText(std::string_view text) {
        std::vector<Entry> entries;
        std::size_t lineNumber = 0;
        std::size_t position = 0;
        while (position <= text.size()) {
            auto end = text.find('\n', position);
            if (end == std::string_view::npos) end = text.size();
            ++lineNumber;
            ParseLine(text.substr(position, end - position), lineNumber, entries);
            position = end + 1;
        }
        return entries;
    }

    static std::vector<Entry> ParseFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("APS5_PAD_SCRIPT: cannot open " + path.string());
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        try {
            return ParseText(text);
        } catch (const std::runtime_error& error) {
            throw std::runtime_error("APS5_PAD_SCRIPT " + path.string() + ": " + error.what());
        }
    }

    void SetScript(std::vector<Entry> entries) {
        script.clear();
        scriptEnd = 0;
        for (auto& entry : entries) {
            const std::uint64_t startUs = entry.startMs * 1000;
            const std::uint64_t endUs = (entry.startMs + entry.holdMs) * 1000;
            scriptEnd = std::max(scriptEnd, endUs);
            script.push_back({std::move(entry), startUs, endUs, -1});
        }
    }

    void SetLoopMs(std::uint64_t value) { loopUs = value * 1000; }

    void SetLive(const std::filesystem::path& path) {
        livePath = path;
        std::error_code error;
        const auto size = std::filesystem::file_size(livePath, error);
        liveOffset = error ? 0 : static_cast<std::uint64_t>(size);
        liveSize = liveOffset;
        liveTime = error ? std::filesystem::file_time_type{} : std::filesystem::last_write_time(livePath, error);
    }

    bool Active() const { return !script.empty() || !livePath.empty(); }

    void Start(std::uint64_t now) {
        if (started) return;
        started = true;
        origin = now;
    }

    bool Started() const { return started; }

    Frame Sample(std::uint64_t now, Report* report = nullptr) {
        Frame frame;
        if (!started) return frame;
        if (!livePath.empty()) pollLive(now, report);
        const std::uint64_t elapsed = now >= origin ? now - origin : 0;
        std::uint64_t cycle = 0;
        std::uint64_t position = elapsed;
        if (loopUs != 0 && scriptEnd != 0) {
            const std::uint64_t length = scriptEnd + loopUs;
            cycle = elapsed / length;
            position = elapsed % length;
        }
        for (auto& event : script) apply(event, position, cycle, elapsed, frame, report);
        for (auto& event : live) apply(event, elapsed, 0, elapsed, frame, report);
        if ((frame.buttons & 0x100) != 0) frame.analogL2 = 255;
        if ((frame.buttons & 0x200) != 0) frame.analogR2 = 255;
        return frame;
    }

    static std::string Describe(std::uint32_t buttons) {
        std::string text;
        for (const auto& name : names()) {
            if ((buttons & name.second) == 0) continue;
            if (!text.empty()) text += '+';
            text += name.first;
        }
        return text;
    }

private:
    struct Scheduled {
        Entry entry;
        std::uint64_t startUs = 0;
        std::uint64_t endUs = 0;
        std::int64_t firedCycle = -1;
    };

    using NameTable = std::array<std::pair<std::string_view, std::uint32_t>, 16>;

    static const NameTable& names() {
        static const NameTable table{{{"cross", 0x4000}, {"circle", 0x2000}, {"square", 0x8000}, {"triangle", 0x1000},
            {"options", 0x8}, {"touchpad", 0x100000}, {"l1", 0x400}, {"r1", 0x800}, {"l2", 0x100}, {"r2", 0x200},
            {"l3", 0x2}, {"r3", 0x4}, {"up", 0x10}, {"down", 0x40}, {"left", 0x80}, {"right", 0x20}}};
        return table;
    }

    static bool buttonBit(std::string_view name, std::uint32_t& bit) {
        for (const auto& entry : names()) {
            if (entry.first == name) { bit = entry.second; return true; }
        }
        return false;
    }

    static bool number(std::string_view token, std::uint64_t& value) {
        if (token.empty()) return false;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        return parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size();
    }

    [[noreturn]] static void fail(std::size_t line, const std::string& message, std::string_view text) {
        throw std::runtime_error("line " + std::to_string(line) + ": " + message + " in '" + std::string(text) + "'");
    }

    static std::uint64_t timeField(std::string_view token, std::size_t line, const char* what, std::string_view text, bool positive) {
        std::uint64_t value = 0;
        if (!number(token, value)) fail(line, std::string(what) + " must be a non-negative integer, got '" + std::string(token) + "'", text);
        if (value > kMaxTimeMs || (positive && value == 0)) fail(line, std::string(what) + " out of range (1.." + std::to_string(kMaxTimeMs) + " ms)", text);
        return value;
    }

    static std::uint32_t buttonField(std::string_view token, std::size_t line, std::string_view text) {
        std::uint32_t mask = 0;
        while (true) {
            const auto plus = token.find('+');
            const auto part = token.substr(0, plus);
            std::uint32_t bit = 0;
            if (!buttonBit(part, bit)) fail(line, "unknown button '" + std::string(part) + "'", text);
            mask |= bit;
            if (plus == std::string_view::npos) return mask;
            token.remove_prefix(plus + 1);
        }
    }

    static void ParseLine(std::string_view raw, std::size_t line, std::vector<Entry>& out) {
        const auto hash = raw.find('#');
        const std::string_view text = hash == std::string_view::npos ? raw : raw.substr(0, hash);
        std::vector<std::string> tokens;
        {
            std::istringstream fields{std::string(text)};
            std::string token;
            while (fields >> token) tokens.push_back(token);
        }
        if (tokens.empty()) return;
        const std::string shown = [&] {
            std::string joined;
            for (const auto& token : tokens) joined += (joined.empty() ? "" : " ") + token;
            return joined;
        }();
        const auto add = [&](Entry entry) {
            if (out.size() >= kMaxEntries) fail(line, "more than " + std::to_string(kMaxEntries) + " events", shown);
            out.push_back(std::move(entry));
        };
        if (tokens[0] == "stick") {
            if (tokens.size() != 6) fail(line, "expected: stick <left|right> <x> <y> <start_ms> <hold_ms>", shown);
            Entry entry;
            if (tokens[1] == "left") entry.stick = 0;
            else if (tokens[1] == "right") entry.stick = 1;
            else fail(line, "stick side must be left or right, got '" + tokens[1] + "'", shown);
            for (int axis = 0; axis < 2; ++axis) {
                std::uint64_t value = 0;
                if (!number(tokens[2 + axis], value) || value > 255) fail(line, "stick x and y must be 0..255, got '" + tokens[2 + axis] + "'", shown);
                entry.axes[axis] = static_cast<std::uint8_t>(value);
            }
            entry.startMs = timeField(tokens[4], line, "start_ms", shown, false);
            entry.holdMs = timeField(tokens[5], line, "hold_ms", shown, true);
            entry.label = "stick " + tokens[1] + " " + tokens[2] + "," + tokens[3];
            add(std::move(entry));
            return;
        }
        std::size_t index = 0;
        std::uint64_t start = 0;
        const bool repeatFirst = tokens[0] == "repeat";
        const bool repeatSecond = tokens.size() > 1 && tokens[1] == "repeat";
        if (repeatFirst || repeatSecond) {
            if (repeatSecond) start = timeField(tokens[0], line, "start_ms", shown, false);
            index = repeatSecond ? 2 : 1;
            if (tokens.size() < index + 3 || tokens.size() > index + 4) fail(line, "expected: [<start_ms>] repeat <every_ms> <count> <button>[+<button>...] [<hold_ms>]", shown);
            const auto every = timeField(tokens[index], line, "every_ms", shown, true);
            std::uint64_t count = 0;
            if (!number(tokens[index + 1], count) || count == 0 || count > kMaxEntries) fail(line, "count must be 1.." + std::to_string(kMaxEntries), shown);
            const auto buttons = buttonField(tokens[index + 2], line, shown);
            const auto hold = tokens.size() == index + 4 ? timeField(tokens[index + 3], line, "hold_ms", shown, true) : kDefaultHoldMs;
            for (std::uint64_t i = 0; i < count; ++i) {
                Entry entry;
                entry.startMs = start + i * every;
                if (entry.startMs > kMaxTimeMs) fail(line, "repeat runs past " + std::to_string(kMaxTimeMs) + " ms", shown);
                entry.holdMs = hold;
                entry.buttons = buttons;
                entry.label = Describe(buttons);
                add(std::move(entry));
            }
            return;
        }
        if (tokens.size() < 2 || tokens.size() > 3) fail(line, "expected: <start_ms> <button>[+<button>...] [<hold_ms>]", shown);
        Entry entry;
        entry.startMs = timeField(tokens[0], line, "start_ms", shown, false);
        entry.buttons = buttonField(tokens[1], line, shown);
        entry.holdMs = tokens.size() == 3 ? timeField(tokens[2], line, "hold_ms", shown, true) : kDefaultHoldMs;
        entry.label = Describe(entry.buttons);
        add(std::move(entry));
    }

    void apply(Scheduled& event, std::uint64_t position, std::uint64_t cycle, std::uint64_t elapsed, Frame& frame, Report* report) {
        if (position < event.startUs) return;
        if (event.firedCycle != static_cast<std::int64_t>(cycle)) {
            event.firedCycle = static_cast<std::int64_t>(cycle);
            if (report != nullptr) report->fired.push_back({elapsed / 1000, event.entry.holdMs, event.entry.label});
        }
        if (position >= event.endUs) return;
        frame.buttons |= event.entry.buttons;
        if (event.entry.stick >= 0) {
            frame.hasSticks = true;
            frame.sticks[event.entry.stick * 2] = event.entry.axes[0];
            frame.sticks[event.entry.stick * 2 + 1] = event.entry.axes[1];
        }
    }

    void pollLive(std::uint64_t now, Report* report) {
        if (now < nextLivePoll) return;
        nextLivePoll = now + kLivePollUs;
        std::error_code error;
        const auto size = std::filesystem::file_size(livePath, error);
        if (error) return;
        const auto time = std::filesystem::last_write_time(livePath, error);
        if (error || (time == liveTime && size == liveSize)) return;
        liveTime = time;
        liveSize = size;
        if (size < liveOffset) liveOffset = 0;
        if (size == liveOffset) return;
        std::ifstream file(livePath, std::ios::binary);
        if (!file) return;
        file.seekg(static_cast<std::streamoff>(liveOffset));
        std::string text(static_cast<std::size_t>(size - liveOffset), '\0');
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
        text.resize(static_cast<std::size_t>(file.gcount()));
        const auto complete = text.rfind('\n');
        if (complete == std::string::npos) return;
        text.resize(complete + 1);
        liveOffset += text.size();
        std::size_t position = 0;
        std::size_t line = 0;
        const std::uint64_t elapsed = now >= origin ? now - origin : 0;
        while (position < text.size()) {
            const auto end = text.find('\n', position);
            ++line;
            std::vector<Entry> parsed;
            try {
                ParseLine(std::string_view(text).substr(position, end - position), line, parsed);
            } catch (const std::runtime_error& failure) {
                if (report != nullptr) report->errors.push_back(std::string("APS5_PAD_SCRIPT_LIVE ") + failure.what());
                parsed.clear();
            }
            for (auto& entry : parsed) {
                const std::uint64_t startUs = elapsed + entry.startMs * 1000;
                const std::uint64_t endUs = startUs + entry.holdMs * 1000;
                live.push_back({std::move(entry), startUs, endUs, -1});
            }
            position = end + 1;
        }
        std::erase_if(live, [&](const Scheduled& event) { return event.endUs + 1000000 < elapsed; });
    }

    std::vector<Scheduled> script;
    std::vector<Scheduled> live;
    std::uint64_t scriptEnd = 0;
    std::uint64_t loopUs = 0;
    bool started = false;
    std::uint64_t origin = 0;
    std::filesystem::path livePath;
    std::uint64_t liveOffset = 0;
    std::uint64_t liveSize = 0;
    std::filesystem::file_time_type liveTime{};
    std::uint64_t nextLivePoll = 0;
};
}

#endif

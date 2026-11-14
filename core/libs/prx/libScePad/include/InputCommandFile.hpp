#ifndef CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Pad {
class InputCommandFile {
public:
    struct Input {
        std::uint32_t buttons = 0;
        bool hasSticks = false;
        std::array<std::uint8_t, 4> sticks{};
        bool touchLeft = false;
        bool touchRight = false;
        bool hasTouch = false;
        std::uint16_t touchX = 0;
        std::uint16_t touchY = 0;
        std::uint8_t touchId = 0;
        bool touchActive = false;

        bool operator==(const Input&) const = default;
    };

    explicit InputCommandFile(const std::filesystem::path& path) : path(path) {
        if (!this->path.empty()) lastSequence = read().sequence;
    }

    Input Poll(std::uint64_t now) {
        if (path.empty()) return {};
        if (now >= nextPoll) {
            nextPoll = now > std::numeric_limits<std::uint64_t>::max() - 100000
                ? std::numeric_limits<std::uint64_t>::max() : now + 100000;
            auto command = read();
            if (command.sequence > lastSequence) {
                lastSequence = command.sequence;
                events = std::move(command.events);
                started = now;
            }
        }
        if (events.empty() || now < started) return {};
        const auto elapsed = now - started;
        const auto next = std::upper_bound(events.begin(), events.end(), elapsed,
            [](std::uint64_t time, const Event& event) { return time < event.start; });
        if (next == events.begin()) return {};
        const auto& event = *std::prev(next);
        return elapsed < event.end ? event.input : Input{};
    }

    std::uint32_t Buttons(std::uint64_t now) { return Poll(now).buttons; }

    std::uint32_t HeldThrough(std::uint64_t since, std::uint64_t now) const {
        if (events.empty() || since < started || now < since) return 0;
        const auto from = since - started;
        const auto to = now - started;
        auto event = std::upper_bound(events.begin(), events.end(), from,
            [](std::uint64_t time, const Event& candidate) { return time < candidate.start; });
        if (event == events.begin()) return 0;
        --event;
        std::uint32_t held = ~0u;
        for (std::uint64_t covered = from; event != events.end() && event->start <= covered; ++event) {
            if (covered >= event->end) return 0;
            held &= event->input.buttons;
            covered = event->end;
            if (to < covered) return held;
        }
        return 0;
    }

private:
    struct Event {
        std::uint64_t start = 0;
        std::uint64_t end = 0;
        Input input;
    };

    struct Command {
        std::uint64_t sequence = 0;
        std::vector<Event> events;
    };

    Command read() const {
#ifdef _WIN32
        std::FILE* file = _wfopen(path.c_str(), L"rb");
#else
        std::FILE* file = std::fopen(path.c_str(), "rb");
#endif
        if (file == nullptr) return {};
        std::string contents;
        char chunk[2048];
        std::size_t got = 0;
        while ((got = std::fread(chunk, 1, sizeof(chunk), file)) != 0) {
            if (contents.size() + got > 64 * 1024) {
                std::fclose(file);
                return {};
            }
            contents.append(chunk, got);
        }
        const bool failed = std::ferror(file) != 0;
        std::fclose(file);
#ifdef _WIN32
        const auto endOfText = contents.find('\x1a');
        if (endOfText != std::string::npos) contents.resize(endOfText);
#endif
        if (failed || contents.empty()) return {};
        std::size_t position = 0;
        bool overlong = false;
        const auto nextLine = [&]() {
            const std::size_t begin = position;
            while (position < contents.size() && contents[position] != '\n') ++position;
            std::size_t end = position;
            if (end > begin && contents[end - 1] == '\r') --end;
            if (end - begin >= 127) overlong = true;
            if (position < contents.size()) ++position;
            const std::string_view line(contents.data() + begin, end - begin);
            return line.substr(0, line.find('\0'));
        };
        const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; };
        const std::string_view text = nextLine();
        if (overlong) return {};
        if (text.find('-') != std::string_view::npos) return {};
        std::string_view tokens[9];
        int count = 0;
        for (std::size_t at = 0; at < text.size();) {
            while (at < text.size() && space(text[at])) ++at;
            if (at == text.size()) break;
            std::size_t end = at;
            while (end < text.size() && !space(text[end])) ++end;
            if (count < 9) tokens[count] = text.substr(at, end - at);
            ++count;
            at = end;
        }
        if (count == 0 || count > 9) return {};
        if (tokens[0] == "APS5_PAD_TIMELINE") {
            Command command;
            if (count != 4) return {};
            std::uint64_t header[3]{};
            for (int i = 0; i < 3; ++i)
                if (!parseUnsigned(tokens[1 + i], header[i])) return {};
            const std::uint64_t version = header[0];
            command.sequence = header[1];
            const std::uint64_t eventCount = header[2];
            if (version != 1 || command.sequence == 0 || eventCount == 0 || eventCount > 4096) return {};
            if (command.sequence <= lastSequence) return command;
            command.events.reserve(static_cast<std::size_t>(eventCount));
            std::uint64_t previousEnd = 0;
            for (std::uint64_t index = 0; index < eventCount; ++index) {
                if (position >= contents.size()) return {};
                const std::string_view eventText = nextLine();
                if (overlong) return {};
                if (eventText.find('-') != std::string_view::npos) return {};
                std::uint64_t row[7]{};
                const int columns = parseRow(eventText, row);
                if (columns != 3 && columns != 7) return {};
                const std::uint64_t start = row[0], duration = row[1], buttons = row[2];
                if ((buttons & ~(0x10fffeull | TouchLeftBit | TouchRightBit)) != 0 || duration == 0 || duration > 5000 ||
                    start < previousEnd || start > 3600000 - duration) return {};
                previousEnd = start + duration;
                Input input;
                input.buttons = static_cast<std::uint32_t>(buttons & 0x10fffeull);
                input.touchLeft = (buttons & TouchLeftBit) != 0;
                input.touchRight = (buttons & TouchRightBit) != 0;
                for (int column = 3; column < columns; ++column) {
                    if (row[column] > 255) return {};
                    input.sticks[column - 3] = static_cast<std::uint8_t>(row[column]);
                }
                input.hasSticks = columns == 7;
                command.events.push_back({start * 1000, previousEnd * 1000, input});
            }
            while (position < contents.size()) {
                if (!space(contents[position])) return {};
                ++position;
            }
            return command;
        }
        Input input;
        if (count == 4 || count == 8) {
            std::string_view touch(tokens[--count]);
            if (!touch.starts_with("touch(") || !touch.ends_with(')')) return {};
            touch.remove_prefix(6);
            touch.remove_suffix(1);
            std::uint64_t coordinates[4]{};
            for (int i = 0; i < 4; ++i) {
                const auto comma = touch.find(',');
                if ((i < 3) != (comma != std::string_view::npos)) return {};
                if (!parseUnsigned(touch.substr(0, comma), coordinates[i])) return {};
                if (i < 3) touch.remove_prefix(comma + 1);
            }
            if (coordinates[0] >= 1920 || coordinates[1] >= 943 || coordinates[2] > 127 || coordinates[3] > 1) return {};
            input.hasTouch = true;
            input.touchX = static_cast<std::uint16_t>(coordinates[0]);
            input.touchY = static_cast<std::uint16_t>(coordinates[1]);
            input.touchId = static_cast<std::uint8_t>(coordinates[2]);
            input.touchActive = coordinates[3] != 0;
        }
        if (count != 3 && count != 7) return {};
        std::uint64_t values[7]{};
        for (int i = 0; i < count; ++i) {
            if (!parseUnsigned(tokens[i], values[i])) return {};
        }
        if (values[0] == 0 || (values[1] & ~(0x10fffeull | TouchLeftBit | TouchRightBit)) != 0 || values[2] == 0 || values[2] > 5000) return {};
        Command command;
        command.sequence = values[0];
        input.buttons = static_cast<std::uint32_t>(values[1] & 0x10fffeull);
        input.touchLeft = (values[1] & TouchLeftBit) != 0;
        input.touchRight = (values[1] & TouchRightBit) != 0;
        for (int i = 3; i < count; ++i) {
            if (values[i] > 255) return {};
            input.sticks[i - 3] = static_cast<std::uint8_t>(values[i]);
        }
        input.hasSticks = count == 7;
        command.events.push_back({0, values[2] * 1000, input});
        return command;
    }

    static int parseRow(std::string_view text, std::uint64_t (&row)[7]) {
        const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; };
        int columns = 0;
        std::size_t index = 0;
        while (index < text.size()) {
            if (space(text[index])) {
                ++index;
                continue;
            }
            const std::size_t first = index;
            if (text[index] == '+') ++index;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9') ++index;
            if (index < text.size() && !space(text[index]) && text[index] != '+') return -1;
            if (columns == 7 || !parseUnsigned(text.substr(first, index - first), row[columns])) return -1;
            ++columns;
        }
        return columns;
    }

    static bool parseUnsigned(std::string_view token, std::uint64_t& value) {
        if (token.empty()) return false;
        const char* first = token.data() + (token.front() == '+' ? 1 : 0);
        const char* last = token.data() + token.size();
        const auto parsed = std::from_chars(first, last, value);
        return first != last && parsed.ec == std::errc{} && parsed.ptr == last;
    }

    static constexpr std::uint64_t TouchLeftBit = 0x200000;
    static constexpr std::uint64_t TouchRightBit = 0x400000;

    std::filesystem::path path;
    std::uint64_t lastSequence = 0;
    std::uint64_t nextPoll = 0;
    std::uint64_t started = 0;
    std::vector<Event> events;
};
}

#endif

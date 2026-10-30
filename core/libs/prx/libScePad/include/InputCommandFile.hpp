#ifndef CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <filesystem>
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
        std::ifstream file(path);
        char line[128]{};
        if (!file.getline(line, sizeof(line))) return {};
        const std::string text(line);
        if (text.find('-') != std::string::npos) return {};
        std::istringstream fields(text);
        std::string first;
        if (!(fields >> first)) return {};
        if (first == "APS5_PAD_TIMELINE") {
            Command command;
            std::string extra;
            std::uint64_t version = 0, count = 0;
            if (!(fields >> version >> command.sequence >> count) || fields >> extra) return {};
            if (version != 1 || command.sequence == 0 || count == 0 || count > 4096) return {};
            if (command.sequence <= lastSequence) return command;
            command.events.reserve(static_cast<std::size_t>(count));
            std::uint64_t previousEnd = 0;
            for (std::uint64_t index = 0; index < count; ++index) {
                if (!file.getline(line, sizeof(line))) return {};
                const std::string eventText(line);
                if (eventText.find('-') != std::string::npos) return {};
                fields.clear();
                fields.str(eventText);
                std::uint64_t start = 0, duration = 0, buttons = 0;
                if (!(fields >> start >> duration >> buttons) || fields >> extra) return {};
                if ((buttons & ~(0x10fffeull | TouchLeftBit | TouchRightBit)) != 0 || duration == 0 || duration > 5000 ||
                    start < previousEnd || start > 3600000 - duration) return {};
                previousEnd = start + duration;
                Input input;
                input.buttons = static_cast<std::uint32_t>(buttons & 0x10fffeull);
                input.touchLeft = (buttons & TouchLeftBit) != 0;
                input.touchRight = (buttons & TouchRightBit) != 0;
                command.events.push_back({start * 1000, previousEnd * 1000, input});
            }
            file >> std::ws;
            if (file.peek() != std::char_traits<char>::eof()) return {};
            return command;
        }
        fields.clear();
        fields.str(text);
        std::string tokens[9];
        int count = 0;
        while (count < 9 && fields >> tokens[count]) ++count;
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

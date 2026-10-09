#ifndef CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INPUTCOMMANDFILE_HPP

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace Pad {
class InputCommandFile {
public:
    explicit InputCommandFile(const char* path) : path(path != nullptr ? path : "") {
        if (!this->path.empty()) lastSequence = read().sequence;
    }

    std::uint32_t Buttons(std::uint64_t now) {
        if (path.empty()) return 0;
        if (now >= nextPoll) {
            nextPoll = now + 100000;
            const auto command = read();
            if (command.sequence > lastSequence) {
                lastSequence = command.sequence;
                buttons = command.buttons;
                until = now + command.durationMs * 1000ull;
            }
        }
        return now < until ? buttons : 0;
    }

private:
    struct Command {
        std::uint64_t sequence = 0;
        std::uint32_t buttons = 0;
        std::uint32_t durationMs = 0;
    };

    Command read() const {
        std::ifstream file(path);
        char line[128]{};
        if (!file.getline(line, sizeof(line))) return {};
        const std::string text(line);
        if (text.find('-') != std::string::npos) return {};
        std::istringstream fields(text);
        Command command;
        std::string extra;
        if (!(fields >> command.sequence >> command.buttons >> command.durationMs) || fields >> extra) return {};
        if (command.sequence == 0 || (command.buttons & ~0x10fffeu) != 0 || command.durationMs == 0 || command.durationMs > 5000) return {};
        return command;
    }

    std::string path;
    std::uint64_t lastSequence = 0;
    std::uint64_t nextPoll = 0;
    std::uint64_t until = 0;
    std::uint32_t buttons = 0;
};
}

#endif

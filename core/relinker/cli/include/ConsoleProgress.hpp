#ifndef CLI_CONSOLEPROGRESS_HPP
#define CLI_CONSOLEPROGRESS_HPP

#include <domain/IProgress.hpp>
#include <ostream>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>

namespace Cli {

class ConsoleProgress final : public Domain::IProgress {
public:
    explicit ConsoleProgress(std::ostream& stream) : stream(stream) {}

    void IncludeFile(std::string_view path) {
        const auto separator = path.find_last_of("/\\");
        const auto name = path.substr(separator == std::string_view::npos ? 0 : separator + 1);
        nameWidth = std::max(nameWidth, textWidth(name));
    }

    void Report(const Domain::ProgressEvent& event) override {
        if (event.Completed > event.Total) throw std::logic_error("Invalid progress counters");
        const char* label = nullptr;
        switch (event.Stage) {
            case Domain::ProgressStage::Executable: label = "Executable"; break;
            case Domain::ProgressStage::Modules: label = "PRX conversion"; break;
            case Domain::ProgressStage::Syscalls: label = "Syscall scan"; break;
            case Domain::ProgressStage::Decode: label = "Instruction scan"; break;
            case Domain::ProgressStage::Lowering: label = "Instruction conversion"; break;
            case Domain::ProgressStage::Imports: label = "Import analysis"; break;
            case Domain::ProgressStage::Image: label = "Output image"; break;
            case Domain::ProgressStage::Write: label = "Write output"; break;
            default: throw std::logic_error("Unknown progress stage");
        }
        const auto separator = event.Input.find_last_of("/\\");
        const auto name = event.Input.substr(separator == std::string::npos ? 0 : separator + 1);
        if (event.Stage == Domain::ProgressStage::Executable) {
            if (event.Completed == 0) Message("Converting " + name + "\n");
            else Finish();
            return;
        }
        if (event.Stage == Domain::ProgressStage::Imports) {
            if (event.Completed == 0) {
                Finish();
                stream << '\n';
                Message("Import analysis\n");
            }
            else Finish();
            return;
        }
        const auto percent = event.Total == 0 ? 100u : static_cast<unsigned>(100.0L * event.Completed / event.Total);
        const bool changed = lastStage != event.Stage || lastInput != event.Input;
        const bool complete = event.Completed == event.Total;
        const auto now = std::chrono::steady_clock::now();
        if (!changed && lastPercent == percent && event.Completed != 0) return;
        if (!changed && !complete && now - lastUpdate < std::chrono::milliseconds(80)) return;
        if (changed || messages) Finish();
        std::ostringstream line;
        line << std::left << std::setw(22) << label;
        line << " | " << name << std::string(nameWidth > textWidth(name) ? nameWidth - textWidth(name) : 0, ' ');
        line << " | " << std::right << std::setw(3) << percent << '%';
        if (event.Stage == Domain::ProgressStage::Modules) line << " (" << event.Completed << '/' << event.Total << ')';
        const auto text = line.str();
        stream << '\r' << text;
        if (lineWidth > text.size()) stream << std::string(lineWidth - text.size(), ' ') << '\r' << text;
        stream << std::flush;
        lineWidth = text.size();
        active = true;
        lastStage = event.Stage;
        lastInput = event.Input;
        lastPercent = percent;
        lastUpdate = now;
        if (complete) Finish();
    }

    void Message(std::string_view message) override {
        if (active) Finish();
        stream << message;
        if (!message.empty() && message.back() != '\n') stream << '\n';
        stream << std::flush;
        messages = true;
    }

    void Finish() {
        if (active || messages) stream << '\n' << std::flush;
        active = false;
        messages = false;
        lineWidth = 0;
    }

private:
    static std::size_t textWidth(std::string_view text) {
        return static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [](unsigned char character) { return (character & 0xc0) != 0x80; }));
    }

    std::size_t nameWidth = 0;
    std::ostream& stream;
    Domain::ProgressStage lastStage = Domain::ProgressStage::Write;
    std::string lastInput;
    unsigned lastPercent = 101;
    std::size_t lineWidth = 0;
    bool active = false;
    bool messages = false;
    std::chrono::steady_clock::time_point lastUpdate{};
};

}

#endif

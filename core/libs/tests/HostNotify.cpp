#include "prx/libkernel/System/include/HostNotify.hpp"
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

class RecordingHostNotifySink final : public IHostNotifySink {
public:
    void Notify(std::string_view title, std::string_view text) override {
        titles.emplace_back(title);
        texts.emplace_back(text);
    }

    std::vector<std::string> titles;
    std::vector<std::string> texts;
};

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "host_notify: %s\n", what);
        std::abort();
    }
}

void RefusesValue(const char* text) {
    Require(!ParseHostNotifyMode(text).has_value(), "an invalid value was accepted");
    Require(HostNotifyModeFromEnvironment(text) == HostNotifyMode::Off, "an invalid value did not stay off");
}

}

void SinkReceivesTitleAndTextAsGiven() {
    RecordingHostNotifySink recording;
    IHostNotifySink& sink = recording;
    sink.Notify("AnyPS5", "The feature is not available.");
    sink.Notify("AnyPS5", "");
    Require(recording.titles.size() == 2, "the sink did not receive two notices");
    Require(recording.titles[0] == "AnyPS5" && recording.texts[0] == "The feature is not available.", "the first notice was changed");
    Require(recording.titles[1] == "AnyPS5" && recording.texts[1].empty(), "an empty text was changed");
}

void LinuxArgvIsOneArgumentPerPart() {
    const auto argv = LinuxNotifyArgv("AnyPS5", "Hello");
    Require(argv == HostNotifyArgv({"notify-send", "--", "AnyPS5", "Hello"}), "the Linux argv is not notify-send -- title text");
}

void LinuxArgvKeepsShellSyntaxAsData() {
    const std::string text = "$(touch /tmp/x) `id` \"quoted\"; rm -rf ~ | 'x' & \\n";
    const auto argv = LinuxNotifyArgv("AnyPS5", text);
    Require(argv.size() == 4, "shell syntax changed the argument count");
    Require(argv[3] == text, "shell syntax in the text was changed");
}

void LinuxArgvKeepsLeadingDashAsText() {
    const auto argv = LinuxNotifyArgv("AnyPS5", "--urgency=critical");
    Require(argv.size() == 4 && argv[1] == "--" && argv[3] == "--urgency=critical", "a text starting with a dash is read as an option");
}

void MacArgvPassesTextAsTrailingArguments() {
    const std::string text = "say \"hi\" \\ $HOME\nnext line";
    const auto argv = MacNotifyArgv("AnyPS5", text);
    Require(argv.size() == 9, "the macOS argv has an unexpected length");
    Require(argv[0] == "osascript" && argv[1] == "-e" && argv[2] == "on run argv", "the macOS argv does not start with the run handler");
    Require(argv[7] == "AnyPS5" && argv[8] == text, "the macOS title and text are not the last two arguments");
    for (std::size_t index = 0; index < 7; ++index) {
        Require(argv[index].find("say") == std::string::npos && argv[index].find("$HOME") == std::string::npos, "user text leaked into the AppleScript source");
    }
}

int main() {
    SinkReceivesTitleAndTextAsGiven();
    LinuxArgvIsOneArgumentPerPart();
    LinuxArgvKeepsShellSyntaxAsData();
    LinuxArgvKeepsLeadingDashAsText();
    MacArgvPassesTextAsTrailingArguments();
    Require(ParseHostNotifyMode(nullptr) == HostNotifyMode::Off, "an unset variable is not off");
    Require(ParseHostNotifyMode("") == HostNotifyMode::Off, "an empty value is not off");
    Require(ParseHostNotifyMode("0") == HostNotifyMode::Off, "0 is not off");
    Require(ParseHostNotifyMode("1") == HostNotifyMode::On, "1 is not on");
    RefusesValue("true");
    RefusesValue("on");
    RefusesValue("yes");
    RefusesValue("2");
    RefusesValue(" 1");
    RefusesValue("1 ");
    RefusesValue("ON");
    return 0;
}

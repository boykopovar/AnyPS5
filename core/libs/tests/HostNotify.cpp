#include "prx/libkernel/System/include/HostNotify.hpp"
#include <cstdio>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
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

void Base64MatchesRfc4648Vectors() {
    Require(HostNotifyBase64("") == "", "empty input");
    Require(HostNotifyBase64("f") == "Zg==", "one byte");
    Require(HostNotifyBase64("fo") == "Zm8=", "two bytes");
    Require(HostNotifyBase64("foo") == "Zm9v", "three bytes");
    Require(HostNotifyBase64("foob") == "Zm9vYg==", "four bytes");
    Require(HostNotifyBase64("fooba") == "Zm9vYmE=", "five bytes");
    Require(HostNotifyBase64("foobar") == "Zm9vYmFy", "six bytes");
    Require(HostNotifyBase64("\xff\xfe") == "//4=", "high bytes");
}

std::string Base64Decode(const std::string& text) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    unsigned buffer = 0;
    int bits = 0;
    for (const char character : text) {
        if (character == '=') break;
        buffer = (buffer << 6) | static_cast<unsigned>(alphabet.find(character));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((buffer >> bits) & 0xff);
        }
    }
    return out;
}

void WindowsArgvCarriesTextOnlyAsBase64() {
    const std::string text = "a \"quoted\" $(calc) '; Remove-Item *";
    const auto argv = WindowsNotifyArgv("AnyPS5", text);
    Require(argv.size() == 7 && argv[0] == "powershell.exe", "the Windows argv does not start with powershell.exe");
    Require(argv[5] == "-EncodedCommand", "the Windows argv does not pass an encoded command");
    for (const auto& argument : argv) {
        Require(argument.find("quoted") == std::string::npos && argument.find("calc") == std::string::npos, "user text appears in a PowerShell argument");
    }
    for (const char character : argv[6]) {
        Require(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=").find(character) != std::string::npos, "the encoded command is not base64");
    }
    std::string script;
    for (const char character : Base64Decode(argv[6])) {
        if (character != '\0') script += character;
    }
    Require(script.find("ShowBalloonTip") != std::string::npos, "the script does not show a balloon tip");
    Require(script.find(HostNotifyBase64(text)) != std::string::npos, "the text is not carried as base64 in the script");
}

void ArgvForBackendMatchesItsBuilder() {
    Require(HostNotifyArgvFor(HostNotifyBackend::NotifySend, "AnyPS5", "x") == LinuxNotifyArgv("AnyPS5", "x"), "notify-send does not use the Linux argv");
    Require(HostNotifyArgvFor(HostNotifyBackend::Osascript, "AnyPS5", "x") == MacNotifyArgv("AnyPS5", "x"), "osascript does not use the macOS argv");
    Require(HostNotifyArgvFor(HostNotifyBackend::PowerShell, "AnyPS5", "x") == WindowsNotifyArgv("AnyPS5", "x"), "PowerShell does not use the Windows argv");
    Require(HostNotifyArgvFor(HostNotifyBackend::None, "AnyPS5", "x").empty(), "no backend still builds an argv");
}

void ExecutableInPathFindsOnlyExecutableDirectories() {
    const auto onlyInB = [](const std::string& candidate) { return candidate == "/b/notify-send"; };
    Require(ExecutableInPath("notify-send", "/a:/b", onlyInB), "a later PATH directory was not searched");
    Require(!ExecutableInPath("notify-send", "/a:/c", onlyInB), "a missing command was found");
    Require(!ExecutableInPath("notify-send", "", onlyInB), "an empty PATH found a command");
    Require(ExecutableInPath("notify-send", ":/b:", onlyInB), "empty PATH entries stop the search");
    Require(!ExecutableInPath("notify-send", "/a/notify-send-extra:/b/notify-send-extra", onlyInB), "a different name matched");
}

#if defined(__linux__)
void LinuxSelectsNotifySendOnlyWhenPresent() {
    const auto everything = [](const std::string&) { return true; };
    const auto nothing = [](const std::string&) { return false; };
    Require(SelectHostNotifyBackend("/usr/bin", everything) == HostNotifyBackend::NotifySend, "notify-send in PATH was not selected");
    Require(SelectHostNotifyBackend("/usr/bin", nothing) == HostNotifyBackend::None, "a missing notify-send was selected");
    Require(SelectHostNotifyBackend("", everything) == HostNotifyBackend::None, "an empty PATH selected notify-send");
}
#endif

void WaitUntil(const std::atomic<int>& counter, int expected) {
    for (int waited = 0; waited < 1000 && counter.load() < expected; ++waited) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void NotifyDoesNotWaitForASleepingBackend() {
    std::atomic<int> calls{0};
    DetachedHostNotifySink sink(HostNotifyBackend::NotifySend, [&calls](const HostNotifyArgv&) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        calls.fetch_add(1);
        return true;
    });
    const auto start = std::chrono::steady_clock::now();
    sink.Notify("AnyPS5", "first");
    sink.Notify("AnyPS5", "second");
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed < std::chrono::milliseconds(500), "Notify waited for the backend");
    WaitUntil(calls, 2);
    Require(calls.load() == 2, "the backend did not run for both notices");
}

void NotifyPassesTheBuiltArgvToTheBackend() {
    std::atomic<int> calls{0};
    HostNotifyArgv seen;
    DetachedHostNotifySink sink(HostNotifyBackend::NotifySend, [&](const HostNotifyArgv& argv) {
        seen = argv;
        calls.fetch_add(1);
        return true;
    });
    sink.Notify("AnyPS5", "hello $(id)");
    WaitUntil(calls, 1);
    Require(calls.load() == 1, "the backend was not run");
    Require(seen == LinuxNotifyArgv("AnyPS5", "hello $(id)"), "the backend did not get the Linux argv");
}

void FailingAndThrowingBackendsDoNotStopTheCaller() {
    std::atomic<int> calls{0};
    DetachedHostNotifySink failing(HostNotifyBackend::NotifySend, [&calls](const HostNotifyArgv&) {
        calls.fetch_add(1);
        return false;
    });
    DetachedHostNotifySink throwing(HostNotifyBackend::Osascript, [&calls](const HostNotifyArgv&) -> bool {
        calls.fetch_add(1);
        throw std::runtime_error("backend failure");
    });
    failing.Notify("AnyPS5", "a");
    throwing.Notify("AnyPS5", "b");
    WaitUntil(calls, 2);
    Require(calls.load() == 2, "a failing backend stopped the caller or the other backend");
}

void OffOrNoBackendMakesNoSinkAndNeverSpawns() {
    std::atomic<int> calls{0};
    const auto spawn = [&calls](const HostNotifyArgv&) {
        calls.fetch_add(1);
        return true;
    };
    Require(MakeHostNotifySink(HostNotifyMode::Off, HostNotifyBackend::NotifySend, spawn) == nullptr, "Off made a sink");
    Require(MakeHostNotifySink(HostNotifyMode::On, HostNotifyBackend::None, spawn) == nullptr, "a missing backend made a sink");
    const auto sink = MakeHostNotifySink(HostNotifyMode::On, HostNotifyBackend::NotifySend, spawn);
    Require(sink != nullptr, "On with a backend made no sink");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(calls.load() == 0, "a sink spawned before any notice");
}

void UnsetVariableIsOffAndAStrayNoticeDoesNotSpawn() {
    Require(HostNotifyModeFromEnvironment(nullptr) == HostNotifyMode::Off, "an unset ANYPS5_HOST_NOTIFY is not off");
    std::atomic<int> calls{0};
    const auto sink = MakeHostNotifySink(HostNotifyModeFromEnvironment(nullptr), HostNotifyBackend::NotifySend, [&calls](const HostNotifyArgv&) {
        calls.fetch_add(1);
        return true;
    });
    Require(sink == nullptr, "the default made a sink");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(calls.load() == 0, "the default spawned a notifier");
}

#ifndef _WIN32
void SpawnReportsTheExitStatusOfTheCommand() {
    Require(SpawnHostNotifyCommand({"true"}), "a successful command was reported as failed");
    Require(!SpawnHostNotifyCommand({"false"}), "a failing command was reported as shown");
    Require(!SpawnHostNotifyCommand({"anyps5-no-such-notifier"}), "a missing command was reported as shown");
    Require(!SpawnHostNotifyCommand({}), "an empty argv was reported as shown");
}
#endif

int main() {
#ifndef _WIN32
    SpawnReportsTheExitStatusOfTheCommand();
#endif
    OffOrNoBackendMakesNoSinkAndNeverSpawns();
    UnsetVariableIsOffAndAStrayNoticeDoesNotSpawn();
    NotifyDoesNotWaitForASleepingBackend();
    NotifyPassesTheBuiltArgvToTheBackend();
    FailingAndThrowingBackendsDoNotStopTheCaller();
    ExecutableInPathFindsOnlyExecutableDirectories();
#if defined(__linux__)
    LinuxSelectsNotifySendOnlyWhenPresent();
#endif
    ArgvForBackendMatchesItsBuilder();
    WindowsArgvCarriesTextOnlyAsBase64();
    Base64MatchesRfc4648Vectors();
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

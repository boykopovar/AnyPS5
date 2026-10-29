#include "prx/libSceSystemService/HostUrl.hpp"
#include "prx/libSceSystemService/HostUrlPolicy.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <stdexcept>
#ifdef _WIN32
#include <stdlib.h>
#endif
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value == nullptr ? "" : value) == 0);
#else
    if (value == nullptr) Require(unsetenv(name) == 0);
    else Require(setenv(name, value, 1) == 0);
#endif
}

void CheckValidator() {
    Require(ValidateHostUrl("http://example.com") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com:8443/a/b?c=d&e=f#g") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[2001:db8::1]:443/x") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[::1]/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("http://a_b.example-site.com?q=%41%20") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com#frag") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("") == HostUrlVerdict::Empty);
}

void CheckScheme() {
    Require(ValidateHostUrl("http:") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("https:") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("http:example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("psno://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("file:///etc/passwd") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("javascript:alert(1)") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("ftp://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("HTTP://example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("//example.com") == HostUrlVerdict::BadScheme);
    Require(ValidateHostUrl("example.com") == HostUrlVerdict::BadScheme);
}

void CheckCharacters() {
    Require(ValidateHostUrl("https://example.com/a b") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\n") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\t") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl(std::string("https://example.com/\0x", 22)) == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\x7f") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\x1b[0m") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\xc3\xa9") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://\xd0\xb0pple.com/") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\\x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/\"x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/<x>") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/`x`") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/|x") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://example.com/{x}") == HostUrlVerdict::BadCharacter);
}

void CheckEscapes() {
    Require(ValidateHostUrl("https://example.com/%") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%4") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%zz") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%00") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%0a") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%0D") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%1F") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%7f") == HostUrlVerdict::BadEscape);
    Require(ValidateHostUrl("https://example.com/%20") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/%7E") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com/%c3%a9") == HostUrlVerdict::Ok);
}

void CheckAuthority() {
    Require(ValidateHostUrl("http://") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https:///path") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://?q=1") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://#x") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://:443/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://exa mple.com/") == HostUrlVerdict::BadCharacter);
    Require(ValidateHostUrl("https://exam!ple.com/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://exam%41ple.com/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[]/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::g]/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::1/") == HostUrlVerdict::BadAuthority);
    Require(ValidateHostUrl("https://[::1]x/") == HostUrlVerdict::BadAuthority);
}

void CheckCredentials() {
    Require(ValidateHostUrl("https://user@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://user:secret@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://@example.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://good.com@evil.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://good.com:443@evil.com/") == HostUrlVerdict::Credentials);
    Require(ValidateHostUrl("https://example.com/path@x") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com?u=a@b") == HostUrlVerdict::Ok);
}

void CheckPort() {
    Require(ValidateHostUrl("https://example.com:/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:0/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:65536/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:123456/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:80a/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://example.com:65535/") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://example.com:1") == HostUrlVerdict::Ok);
    Require(ValidateHostUrl("https://[::1]:/") == HostUrlVerdict::BadPort);
    Require(ValidateHostUrl("https://[::1]:99999/") == HostUrlVerdict::BadPort);
}

void CheckLength() {
    const std::string prefix = "https://example.com/";
    Require(ValidateHostUrl(prefix + std::string(HOST_URL_MAX_LENGTH - prefix.size(), 'a')) == HostUrlVerdict::Ok);
    Require(ValidateHostUrl(prefix + std::string(HOST_URL_MAX_LENGTH - prefix.size() + 1, 'a')) == HostUrlVerdict::TooLong);
    Require(ValidateHostUrl(std::string(100000, 'a')) == HostUrlVerdict::TooLong);
}

bool ModeThrows(const char* value, std::string& message) {
    try {
        static_cast<void>(ParseOpenUrlMode(value, "caller"));
    } catch (const std::runtime_error& error) {
        message = error.what();
        return true;
    }
    return false;
}

void CheckMode() {
    Require(ParseOpenUrlMode(nullptr, "caller") == OpenUrlMode::Never);
    Require(ParseOpenUrlMode("", "caller") == OpenUrlMode::Never);
    Require(ParseOpenUrlMode("never", "caller") == OpenUrlMode::Never);
    Require(ParseOpenUrlMode("ask", "caller") == OpenUrlMode::Ask);
    Require(ParseOpenUrlMode("always", "caller") == OpenUrlMode::Always);
    for (const char* invalid : {"Ask", "ALWAYS", "1", "yes", "ask ", " ask", "never,ask", "0", "on"}) {
        std::string message;
        Require(ModeThrows(invalid, message));
        Require(message.find("ANYPS5_OPEN_URLS") != std::string::npos);
        Require(message.find("caller") != std::string::npos);
        Require(message.find(invalid) != std::string::npos);
    }
    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
    Require(OpenUrlModeFromEnvironment("caller") == OpenUrlMode::Never);
    SetEnvironment("ANYPS5_OPEN_URLS", "");
    Require(OpenUrlModeFromEnvironment("caller") == OpenUrlMode::Never);
    SetEnvironment("ANYPS5_OPEN_URLS", "ask");
    Require(OpenUrlModeFromEnvironment("caller") == OpenUrlMode::Ask);
    SetEnvironment("ANYPS5_OPEN_URLS", "always");
    Require(OpenUrlModeFromEnvironment("caller") == OpenUrlMode::Always);
    SetEnvironment("ANYPS5_OPEN_URLS", "bogus");
    bool threw = false;
    try {
        static_cast<void>(OpenUrlModeFromEnvironment("caller"));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
}

class RecordingLauncher final : public IUrlLauncher {
public:
    bool Launch(const std::string& url) override {
        launched.push_back(url);
        return result;
    }
    std::vector<std::string> launched;
    bool result = true;
};

class ScriptedConfirmation final : public IUrlConfirmation {
public:
    bool Confirm(const std::string& url) override {
        asked.push_back(url);
        return answer;
    }
    std::vector<std::string> asked;
    bool answer = true;
};

void CheckPolicy() {
    const std::string good = "https://example.com/a?b=c";
    RecordingLauncher launcher;
    ScriptedConfirmation confirmation;

    Require(OpenUrlOnHost(OpenUrlMode::Never, good, launcher, confirmation) == OpenUrlOutcome::Disabled);
    Require(launcher.launched.empty() && confirmation.asked.empty());

    Require(OpenUrlOnHost(OpenUrlMode::Ask, good, launcher, confirmation) == OpenUrlOutcome::Launched);
    Require(confirmation.asked == std::vector<std::string>{good});
    Require(launcher.launched == std::vector<std::string>{good});

    confirmation.answer = false;
    Require(OpenUrlOnHost(OpenUrlMode::Ask, good, launcher, confirmation) == OpenUrlOutcome::Declined);
    Require(confirmation.asked.size() == 2 && launcher.launched.size() == 1);
    confirmation.answer = true;

    launcher.result = false;
    Require(OpenUrlOnHost(OpenUrlMode::Ask, good, launcher, confirmation) == OpenUrlOutcome::Failed);
    Require(confirmation.asked.size() == 3 && launcher.launched.size() == 2);
    launcher.result = true;

    confirmation.answer = false;
    Require(OpenUrlOnHost(OpenUrlMode::Always, good, launcher, confirmation) == OpenUrlOutcome::Launched);
    Require(confirmation.asked.size() == 3 && launcher.launched.size() == 3);
    confirmation.answer = true;

    for (const std::string& bad : {std::string("psno://example.com"), std::string("https://u:p@example.com/"), std::string("https://example.com/\n"),
                                   std::string("http://"), std::string("")}) {
        for (const OpenUrlMode mode : {OpenUrlMode::Ask, OpenUrlMode::Always}) {
            const std::size_t asked = confirmation.asked.size();
            const std::size_t launched = launcher.launched.size();
            Require(OpenUrlOnHost(mode, bad, launcher, confirmation) == OpenUrlOutcome::Rejected);
            Require(confirmation.asked.size() == asked && launcher.launched.size() == launched);
        }
        const std::size_t launched = launcher.launched.size();
        Require(OpenUrlOnHost(OpenUrlMode::Never, bad, launcher, confirmation) == OpenUrlOutcome::Disabled);
        Require(launcher.launched.size() == launched);
    }
}

#ifndef _WIN32
std::filesystem::path MakeScratchDirectory() {
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-open-url-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(directory));
    return directory;
}

std::filesystem::path WriteOpener(const std::filesystem::path& directory) {
    const auto opener = directory / "opener.sh";
    {
        std::ofstream script(opener);
        script << "#!/bin/sh\nprintf '%s\\n' \"$#\" \"$0\" \"$1\" > \"" << directory.string() << "/record.tmp\"\nmv \"" << directory.string() << "/record.tmp\" \""
               << directory.string() << "/record\"\n";
    }
    std::filesystem::permissions(opener, std::filesystem::perms::owner_all);
    return opener;
}

std::vector<std::string> WaitForRecord(const std::filesystem::path& directory) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        if (std::filesystem::exists(directory / "record")) {
            std::ifstream record(directory / "record");
            std::vector<std::string> lines;
            for (std::string line; std::getline(record, line);) lines.push_back(line);
            return lines;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::abort();
}

void CheckPosixLauncher() {
    const auto directory = MakeScratchDirectory();
    const auto opener = WriteOpener(directory);
    const std::string url = "https://example.com/$(touch_pwned);id&x=1#a*b";
    {
        HostBrowserLauncher launcher(opener.string());
        Require(launcher.Launch(url));
    }
    const auto lines = WaitForRecord(directory);
    Require(lines.size() == 3);
    Require(lines[0] == "1");
    Require(lines[1] == opener.string());
    Require(lines[2] == url);
    Require(!std::filesystem::exists(directory / "touch_pwned"));
    Require(!std::filesystem::exists(std::filesystem::current_path() / "touch_pwned"));

    HostBrowserLauncher missing((directory / "absent").string());
    Require(!missing.Launch("https://example.com/"));
    HostBrowserLauncher empty{std::string()};
    Require(!empty.Launch("https://example.com/"));
    HostBrowserLauncher guarded(opener.string());
    std::filesystem::remove(directory / "record");
    Require(!guarded.Launch("-https://example.com/"));
    Require(!guarded.Launch(""));
    Require(!guarded.Launch("file:///etc/passwd"));
    Require(!guarded.Launch("https://user@example.com/"));
    Require(!guarded.Launch("https://example.com/ x"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(!std::filesystem::exists(directory / "record"));
    std::filesystem::remove_all(directory);
}
#endif

}

int main() {
    CheckValidator();
    CheckScheme();
    CheckCharacters();
    CheckEscapes();
    CheckAuthority();
    CheckCredentials();
    CheckPort();
    CheckLength();
    CheckMode();
    CheckPolicy();
#ifndef _WIN32
    CheckPosixLauncher();
#endif
    return 0;
}

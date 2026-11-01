#include "GuestSystemServiceOpenUrlFakes.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/HostUrl.hpp"
#include "prx/libSceSystemService/HostUrlPolicy.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include <algorithm>
#include <atomic>
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

std::size_t CountStarted(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.path().filename().string().rfind("started.", 0) == 0) ++count;
    return count;
}

void CheckPosixOutstandingBound() {
    const auto directory = MakeScratchDirectory();
    const auto opener = directory / "blocking.sh";
    {
        std::ofstream script(opener);
        script << "#!/bin/sh\ntouch \"" << directory.string() << "/started.$$\"\nwhile [ ! -e \"" << directory.string()
               << "/release\" ]; do sleep 0.05; done\n";
    }
    std::filesystem::permissions(opener, std::filesystem::perms::owner_all);
    HostBrowserLauncher launcher(opener.string());
    Require(HOST_BROWSER_MAX_OPENERS == 2);
    Require(launcher.Launch("https://example.com/1"));
    Require(launcher.Launch("https://example.com/2"));
    for (int index = 0; index < 20; ++index) Require(!launcher.Launch("https://example.com/3"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Require(CountStarted(directory) == 2);

    { std::ofstream release(directory / "release"); }
    bool recovered = false;
    for (int attempt = 0; attempt < 500 && !recovered; ++attempt) {
        recovered = launcher.Launch("https://example.com/4");
        if (!recovered) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(recovered);
    for (int attempt = 0; attempt < 500 && CountStarted(directory) < 3; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(CountStarted(directory) == 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::filesystem::remove_all(directory);
}

#endif

void CheckConfirmationText() {
    const std::string url = "https://example.com/" + std::string(500, 'a') + "?q=1";
    const std::string text = HostConfirmationText(url);
    std::size_t lineLength = 0;
    std::size_t longest = 0;
    for (const char value : text) {
        if (value == '\n') {
            longest = std::max(longest, lineLength);
            lineLength = 0;
        } else {
            ++lineLength;
        }
    }
    Require(longest <= 96);
    std::string stripped;
    for (const char value : text)
        if (value != '\n') stripped += value;
    Require(stripped.find(url) != std::string::npos);
    Require(HostConfirmationText("https://example.com/").find("https://example.com/") != std::string::npos);
}

#ifdef __linux__
void CheckConfirmationWithoutDisplay() {
    SetEnvironment("DISPLAY", nullptr);
    SetEnvironment("WAYLAND_DISPLAY", nullptr);
    SetEnvironment("SDL_VIDEODRIVER", nullptr);
    HostMessageBoxConfirmation confirmation;
    Require(!confirmation.Confirm("https://example.com/"));
}
#endif

void CheckLaunchWebBrowser() {
    constexpr int unavailable = static_cast<int>(0x8002002Du);
    const std::string good = "https://example.com/store?id=1";
    RecordingLauncher launcher;
    ScriptedConfirmation confirmation;
    FakeClock clock;
    SystemServiceSetUrlHandlers_nid_no_patch(&launcher, &confirmation, &clock);
    const auto call = [&](const char* uri) {
        const int result = sceSystemServiceLaunchWebBrowser(uri);
        clock.Advance(std::chrono::seconds(700));
        return result;
    };

    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
    Require(call(good.c_str()) == unavailable);
    SetEnvironment("ANYPS5_OPEN_URLS", "never");
    Require(call(good.c_str()) == unavailable);
    Require(launcher.launched.empty() && confirmation.asked.empty());

    SetEnvironment("ANYPS5_OPEN_URLS", "ask");
    Require(call(good.c_str()) == 0);
    Require(confirmation.asked == std::vector<std::string>{good} && launcher.launched == std::vector<std::string>{good});
    confirmation.answer = false;
    Require(call(good.c_str()) == unavailable);
    Require(confirmation.asked.size() == 2 && launcher.launched.size() == 1);
    confirmation.answer = true;
    launcher.result = false;
    Require(call(good.c_str()) == unavailable);
    launcher.result = true;
    Require(launcher.launched.size() == 2);

    SetEnvironment("ANYPS5_OPEN_URLS", "always");
    confirmation.answer = false;
    Require(call(good.c_str()) == 0);
    Require(confirmation.asked.size() == 3 && launcher.launched.size() == 3);
    confirmation.answer = true;

    const std::size_t asked = confirmation.asked.size();
    const std::size_t launched = launcher.launched.size();
    for (const char* refused : {"psno:", "http:", "https:", "http://", "https://user:pw@example.com/", "http://exa mple.com/", "https://example.com:0/", "https://example.com/%00"})
        Require(call(refused) == unavailable);
    Require(call("") == static_cast<int>(0x80a10008));
    Require(call("HTTP://example.com/") == static_cast<int>(0x80a10008));
    Require(call("file:///etc/passwd") == static_cast<int>(0x80a10008));
    Require(call(nullptr) == static_cast<int>(0x80a10003));
    const std::string tooLong = "https://example.com/" + std::string(5990, 'a');
    Require(call(tooLong.c_str()) == unavailable);
    const std::string overLimit = "https://example.com/" + std::string(6000, 'a') + std::string(100, 'a');
    Require(call(overLimit.c_str()) == static_cast<int>(0x80a10007));
    Require(confirmation.asked.size() == asked && launcher.launched.size() == launched);

    SetEnvironment("ANYPS5_OPEN_URLS", "bogus");
    bool threw = false;
    try {
        static_cast<void>(call(good.c_str()));
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("ANYPS5_OPEN_URLS") != std::string::npos;
    }
    Require(threw);
    Require(call("") == static_cast<int>(0x80a10008));
    Require(launcher.launched.size() == launched);

    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
    SystemServiceSetUrlHandlers_nid_no_patch(nullptr, nullptr, nullptr);
    Require(call(good.c_str()) == unavailable);
}

void CheckExportedLimits() {
    constexpr int unavailable = static_cast<int>(0x8002002Du);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    FakeClock clock;
    SystemServiceSetUrlHandlers_nid_no_patch(&launcher, &confirmation, &clock);
    SetEnvironment("ANYPS5_OPEN_URLS", "ask");

    confirmation.answer = false;
    Require(sceSystemServiceLaunchWebBrowser("https://example.com/a") == unavailable);
    for (int index = 0; index < 200; ++index)
        Require(sceSystemServiceLaunchWebBrowser(index % 2 == 0 ? "https://example.com/a" : "https://example.org/b") == unavailable);
    Require(confirmation.Count() == 1 && launcher.Count() == 0);
    clock.Advance(OPEN_URL_DECLINE_COOLDOWN);
    confirmation.answer = true;
    Require(sceSystemServiceLaunchWebBrowser("https://example.com/a") == 0);
    Require(confirmation.Count() == 2 && launcher.Count() == 1);

    SetEnvironment("ANYPS5_OPEN_URLS", "always");
    Require(sceSystemServiceLaunchWebBrowser("https://example.com/b") == 0);
    Require(sceSystemServiceLaunchWebBrowser("https://example.com/c") == 0);
    for (int index = 0; index < 200; ++index) Require(sceSystemServiceLaunchWebBrowser("https://example.com/d") == unavailable);
    Require(launcher.Count() == OPEN_URL_MAX_ATTEMPTS);
    clock.Advance(OPEN_URL_WINDOW);
    Require(sceSystemServiceLaunchWebBrowser("https://example.com/e") == 0);
    Require(launcher.Count() == OPEN_URL_MAX_ATTEMPTS + 1);

    SetEnvironment("ANYPS5_OPEN_URLS", "never");
    const std::size_t asked = confirmation.Count();
    const std::size_t launched = launcher.Count();
    for (int index = 0; index < 1000; ++index) Require(sceSystemServiceLaunchWebBrowser("https://example.com/f") == unavailable);
    Require(confirmation.Count() == asked && launcher.Count() == launched);
    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
    SystemServiceSetUrlHandlers_nid_no_patch(nullptr, nullptr, nullptr);
}

void CheckExportedConcurrentCallers() {
    constexpr int unavailable = static_cast<int>(0x8002002Du);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    FakeClock clock;
    SystemServiceSetUrlHandlers_nid_no_patch(&launcher, &confirmation, &clock);
    SetEnvironment("ANYPS5_OPEN_URLS", "ask");
    confirmation.blocking = true;

    std::atomic<int> first{-1};
    std::thread active([&] { first = sceSystemServiceLaunchWebBrowser("https://example.com/a"); });
    confirmation.WaitUntilAsked(1);
    std::vector<std::thread> callers;
    std::atomic<int> unavailableCount{0};
    for (int index = 0; index < 16; ++index)
        callers.emplace_back([&, index] {
            const std::string url = "https://example.com/" + std::to_string(index % 3);
            if (sceSystemServiceLaunchWebBrowser(url.c_str()) == unavailable) ++unavailableCount;
        });
    for (std::thread& caller : callers) caller.join();
    Require(unavailableCount == 16);
    Require(confirmation.Count() == 1 && launcher.Count() == 0);
    confirmation.Release();
    active.join();
    Require(first == 0);
    Require(confirmation.Count() == 1 && launcher.Count() == 1);
    SetEnvironment("ANYPS5_OPEN_URLS", nullptr);
    SystemServiceSetUrlHandlers_nid_no_patch(nullptr, nullptr, nullptr);
}

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
    CheckLaunchWebBrowser();
    CheckExportedLimits();
    CheckExportedConcurrentCallers();
    CheckConfirmationText();
#ifdef __linux__
    CheckConfirmationWithoutDisplay();
#endif
#ifndef _WIN32
    CheckPosixLauncher();
    CheckPosixOutstandingBound();
#endif
    return 0;
}

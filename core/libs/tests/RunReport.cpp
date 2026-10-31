#include "prx/libc/include/General.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

struct NotRun : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int passed = 0, failed = 0, notRun = 0;
void Run(const char* name, void (*test)()) {
    if (const char* only = std::getenv("RR_ONLY"); only != nullptr && std::string(only) != name) return;
    try {
        test();
        ++passed;
        std::printf("PASS %s\n", name);
    } catch (const NotRun& reason) {
        ++notRun;
        std::printf("NOT_RUN %s: %s\n", name, reason.what());
    } catch (const std::exception& error) {
        ++failed;
        std::printf("FAIL %s: %s\n", name, error.what());
    }
}

std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

struct Directory {
    Directory() {
        path = fs::temp_directory_path() / ("anyps5-run-report-" + RunReport::GenerateRunId().substr(0, 12));
        fs::create_directories(path);
    }
    ~Directory() {
        std::error_code error;
        fs::remove_all(path, error);
    }
    fs::path path;
};

constexpr const char* IdA = "0123456789abcdef0123456789abcdef";
constexpr const char* IdB = "fedcba9876543210fedcba9876543210";

RunReport::Settings SettingsFor(const fs::path& executable, const fs::path& user, const char* id) {
    RunReport::Settings settings;
    settings.executableDirectory = executable;
    settings.userStateDirectory = user;
    settings.runId = id;
    return settings;
}

std::string FlagText(const std::vector<std::string>& arguments) {
    const auto flag = RunReport::FlagFromArguments(arguments);
    return flag ? std::string(*flag) : "<none>";
}

void ToggleTests() {
    using RunReport::ResolveToggle;
    const auto flag = [](const char* value) { return std::optional<std::string_view>(value); };
    const std::optional<std::string_view> none;
    Require(ResolveToggle(none, none).enabled && !ResolveToggle(none, none).explicitlyOn, "the default is not on");
    Require(!ResolveToggle(flag("off"), none).enabled, "--aps5-report=off did not turn the report off");
    Require(ResolveToggle(flag("on"), none).enabled && ResolveToggle(flag("on"), none).explicitlyOn, "--aps5-report=on did not turn the report on");
    Require(!ResolveToggle(none, flag("0")).enabled, "APS5_REPORT=0 did not turn the report off");
    Require(ResolveToggle(none, flag("1")).explicitlyOn, "APS5_REPORT=1 did not turn the report on");
    Require(ResolveToggle(flag("on"), flag("0")).enabled, "the environment variable beat the flag");
    Require(!ResolveToggle(flag("off"), flag("1")).enabled, "the environment variable beat the flag");
    const auto invalid = ResolveToggle(flag("maybe"), flag("0"));
    Require(!invalid.enabled && invalid.warnings.size() == 1, "an invalid flag did not warn and fall to the environment variable");
    const auto invalidBoth = ResolveToggle(flag("yes"), flag("2"));
    Require(invalidBoth.enabled && !invalidBoth.explicitlyOn && invalidBoth.warnings.size() == 2, "two invalid values did not warn twice and fall to the default");
    Require(ResolveToggle(std::nullopt, flag("off")).enabled && ResolveToggle(std::nullopt, flag("off")).warnings.size() == 1, "APS5_REPORT=off is not the documented 0|1");
    Require(ResolveToggle(flag("1"), none).enabled && ResolveToggle(flag("1"), none).warnings.size() == 1, "--aps5-report=1 is not the documented on|off");
    const std::vector<std::string> arguments{"game", "--other", "--aps5-report=off", "--aps5-report=on", "--aps5-reports=off"};
    Require(FlagText(arguments) == "on", "the last --aps5-report flag did not win");
    Require(FlagText({"game", "--aps5-reports=off", "aps5-report=off"}) == "<none>", "a look-alike argument was taken for the flag");
}

void PrivacyTests() {
    RunReport::HostIdentifiers host;
    host.words = {"canaryuser", "canary-host-7", "/home/canaryuser"};
    for (const char* safe : {"Example Game", "Ejemplo del Juego \xc3\x9c" "n\xc3\xaf" "code", "Title 2: The Sequel (1.2)", "01.002.000"}) {
        Require(RunReport::SafeText(safe, host).has_value(), std::string("a safe text was dropped: ") + safe);
    }
    for (const char* unsafe : {"played by canaryuser", "CANARYUSER's game", "on canary-host-7", "saved in /home/canaryuser/saves", "see /home/someone/x", "C:\\Users\\someone\\x", "\\\\server\\share",
                               "connect 192.168.1.20", "mac 00:1a:2b:3c:4d:5e", "mac 00-1a-2b-3c-4d-5e", "v6 fe80:0:0:0:1:2:3:4", "line\nbreak", "tab\there", "", "bad utf8 \xc3\x28", "overlong \xc0\xaf"}) {
        Require(!RunReport::SafeText(unsafe, host).has_value(), std::string("an unsafe text was kept: ") + unsafe);
    }
    Require(RunReport::SafeText(std::string(300, 'z'), host).has_value() && !RunReport::SafeText(std::string(301, 'z'), host).has_value(), "the 300 character cap is wrong");
    std::string wide;
    for (int index = 0; index < 300; ++index) wide += "\xc3\xa9";
    Require(RunReport::SafeText(wide, host).has_value(), "300 two-byte characters were refused");
    Require(!RunReport::SafeText(wide + "\xc3\xa9", host).has_value(), "301 two-byte characters were kept");
}

RunReport::Report SampleReport() {
    RunReport::Report report;
    report.generated = "2026-10-29T12:00:00Z";
    report.build.commit = "40f2a9c3d1b7e5864a0c9d2e7f13b5a8c6d49e01";
    report.build.release = "v0.1.3";
    report.build.modified = false;
    report.game.titleId = "PPSA12345";
    report.game.contentVersion = "01.002.000";
    report.game.name = "Example \"Game\"";
    report.os = "linux";
    report.runId = IdA;
    report.seconds = 12.5;
    report.exitCode = 0;
    return report;
}

void SerializeTests() {
    const auto text = RunReport::Serialize(SampleReport());
    Require(text.size() < RunReport::MaxBytes && text.back() == '\n', "the serialized report is the wrong size");
    for (const char* part : {"{\n  \"format\": \"anyps5-report\",\n  \"version\": 1,\n  \"generated\": \"2026-10-29T12:00:00Z\",\n  \"anyps5\": {\n    \"commit\": \"40f2a9c3d1b7e5864a0c9d2e7f13b5a8c6d49e01\",\n    \"release\": \"v0.1.3\",\n    \"modified\": false\n  },\n",
                             "  \"game\": {\n    \"titleId\": \"PPSA12345\",\n    \"contentId\": null,\n    \"contentVersion\": \"01.002.000\",\n    \"name\": \"Example \\\"Game\\\"\"\n  },\n",
                             "  \"system\": {\n    \"os\": \"linux\",\n    \"osVersion\": null,\n    \"cpu\": null,\n    \"ramGiB\": null,\n    \"gpu\": null\n  },\n",
                             "  \"run\": {\n    \"id\": \"0123456789abcdef0123456789abcdef\",\n    \"seconds\": 12.500,\n    \"exit\": \"clean\",\n    \"exitCode\": 0,\n    \"stage\": \"unknown\",\n    \"milestones\": {},\n    \"framesPresented\": 0,\n    \"averageFps\": null,\n    \"fpsWindowSeconds\": null,\n    \"recovered\": false,\n    \"incomplete\": false\n  },\n",
                             "  \"coverage\": {\n    \"milestones\": false,\n    \"missing\": false,\n    \"shaders\": false,\n    \"unsupported\": false,\n    \"imageFormats\": false,\n    \"descriptors\": false\n  },\n",
                             "  \"truncated\": {\n    \"functions\": 0,\n    \"unsupported\": 0,\n    \"imageFormats\": 0,\n    \"descriptors\": 0\n  },\n  \"missing\": {\n    \"functions\": []\n  },\n",
                             "  \"shaders\": {\n    \"compiled\": 0,\n    \"failed\": 0,\n    \"unsupported\": [],\n    \"imageFormats\": [],\n    \"descriptors\": []\n  },\n",
                             "  \"stop\": {\n    \"kind\": \"none\",\n    \"reason\": \"guest-exit\",\n    \"message\": \"The game exited normally.\",\n    \"source\": null\n  }\n}\n"}) {
        Require(Contains(text, part), std::string("the serialized report lacks: ") + part);
    }
    auto unknown = SampleReport();
    unknown.build = {};
    unknown.game = {};
    unknown.os.reset();
    unknown.exitCode.reset();
    const auto unknownText = RunReport::Serialize(unknown);
    for (const char* part : {"\"commit\": null", "\"release\": null", "\"modified\": null", "\"titleId\": null", "\"contentVersion\": null", "\"name\": null", "\"os\": null", "\"exitCode\": null"}) {
        Require(Contains(unknownText, part), std::string("an unknown value was not null: ") + part);
    }
    auto crash = SampleReport();
    crash.exit = RunReport::Exit::Abort;
    crash.stop = {RunReport::Kind::Assert, RunReport::Reason::Crash, std::string("core/a.cpp:12")};
    const auto crashText = RunReport::Serialize(crash);
    Require(Contains(crashText, "\"kind\": \"assert\"") && Contains(crashText, "\"reason\": \"crash\"") && Contains(crashText, "\"message\": \"A runtime assertion failed.\"") && Contains(crashText, "\"source\": \"core/a.cpp:12\""), "an assertion stop was not described");
    auto controls = SampleReport();
    controls.game.name = std::string("a\"b\\c");
    Require(Contains(RunReport::Serialize(controls), "\"name\": \"a\\\"b\\\\c\""), "a name was not escaped");
    controls.game.name = std::string("a\x01" "b");
    Require(Contains(RunReport::Serialize(controls), "\"name\": null"), "a name with a control character was written");
}
bool SamePath(const fs::path& left, const fs::path& right) {
    std::error_code error;
    return fs::weakly_canonical(left, error) == fs::weakly_canonical(right, error);
}

void FinishTests() {
    Directory executable, user;
    RunReport::Collector collector(SettingsFor(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    Require(SamePath(collector.ReportPath(), executable.path / "anyps5-report.json"), "the report is not next to the executable");
    Require(!fs::exists(collector.ReportPath()), "Start wrote a file");
    collector.Finish(RunReport::Exit::Clean, {RunReport::Kind::None, RunReport::Reason::GuestExit, std::nullopt}, 3);
    const auto report = Read(collector.ReportPath());
    Require(Contains(report, "\"exit\": \"clean\"") && Contains(report, "\"exitCode\": 3") && Contains(report, "\"reason\": \"guest-exit\"") && Contains(report, "\"recovered\": false"), "the finished report is wrong: " + report);
    collector.Finish(RunReport::Exit::Abort, {RunReport::Kind::Other, RunReport::Reason::Crash, std::nullopt}, std::nullopt);
    Require(Read(collector.ReportPath()) == report, "a second finish changed the report");
    std::size_t files = 0;
    for (const auto& entry : fs::directory_iterator(executable.path)) { static_cast<void>(entry); ++files; }
    Require(files == 1, "a temporary file was left behind");
}

void ConcurrentFinishTests() {
    Directory executable, user;
    RunReport::Collector collector(SettingsFor(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&collector, index] { collector.Finish(RunReport::Exit::Clean, {RunReport::Kind::None, RunReport::Reason::GuestExit, std::nullopt}, index); });
    }
    for (auto& thread : threads) thread.join();
    Require(collector.Finished() && fs::exists(collector.ReportPath()), "concurrent finishes did not leave one report");
}

void FallbackTests() {
    Directory root, user;
    std::ofstream(root.path / "file") << "not a directory";
    RunReport::Collector collector(SettingsFor(root.path / "file" / "below", user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    Require(SamePath(collector.ReportPath(), user.path / "reports" / IdA / "anyps5-report.json"), "the fallback report is not under reports/<run id>/");
    Require(fs::exists(collector.ReportPath()), "the fallback report was not written");
    RunReport::Collector nowhere(SettingsFor(root.path / "file" / "below", root.path / "file" / "also", IdB));
    Require(nowhere.Start(), "a collector with a folder to try did not start");
    nowhere.Finish(RunReport::Exit::Clean, {}, 0);
    Require(nowhere.Finished() && !fs::exists(root.path / "file" / "below") && !fs::exists(root.path / "file" / "also"), "a report was made where no folder can exist");
    RunReport::Collector none(SettingsFor({}, {}, IdB));
    Require(!none.Start(), "a collector with no folder at all started");
}

void AtomicTests() {
    Directory directory;
    const auto target = directory.path / "anyps5-report.json";
    std::string error;
    Require(RunReport::WriteAtomically(target, "first\n", "aaaaaaaa", error) && Read(target) == "first\n", "a first write failed");
    Require(RunReport::WriteAtomically(target, "second\n", "bbbbbbbb", error) && Read(target) == "second\n", "the old report was not replaced");
#ifndef _WIN32
    fs::create_symlink(directory.path / "elsewhere", directory.path / "link.json");
    std::ofstream(directory.path / "elsewhere") << "keep";
    Require(!RunReport::WriteAtomically(directory.path / "link.json", "x\n", "cccccccc", error) && Read(directory.path / "elsewhere") == "keep", "a symbolic link destination was written through");
    fs::create_directory(directory.path / "folder.json");
    Require(!RunReport::WriteAtomically(directory.path / "folder.json", "x\n", "dddddddd", error), "a folder destination was replaced");
#endif
    Require(!fs::exists(directory.path / ".anyps5-report.json.aaaaaaaa.tmp"), "a temporary file was left behind");
}

void CanaryTests() {
    Directory executable, user;
    auto settings = SettingsFor(executable.path, user.path, IdA);
    settings.game = [] {
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        game.contentVersion = "01.002.000";
        game.name = "Game of " + std::string(std::getenv("USER") != nullptr && std::string(std::getenv("USER")).size() >= 3u ? std::getenv("USER") : "canaryuser") + " at 10.1.2.3";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    Require(Contains(text, "\"name\": null") && !Contains(text, "10.1.2.3") && Contains(text, "\"titleId\": \"PPSA12345\""), "a name with a host identifier was copied: " + text);
    auto invalid = SettingsFor(executable.path, user.path, IdB);
    invalid.game = [] {
        RunReport::GameInfo game;
        game.titleId = "ppsa1";
        game.contentVersion = "01.002 000";
        return game;
    };
    RunReport::Collector other(std::move(invalid));
    Require(other.Start(), "the second collector did not start");
    other.Finish(RunReport::Exit::Clean, {}, 0);
    const auto otherText = Read(other.ReportPath());
    Require(Contains(otherText, "\"titleId\": null") && Contains(otherText, "\"contentVersion\": null"), "an invalid title id or version was copied");
}

void FlagFromTemporaryTest() {
    const auto flag = RunReport::FlagFromArguments(std::vector<std::string>{"game", "--aps5-report=off"});
    std::vector<std::string> churn;
    for (int index = 0; index < 64; ++index) churn.emplace_back(64, 'x');
    Require(flag && std::string(*flag) == "off", "the flag text did not survive its argument vector");
}

void FinishWhileProviderBlockedTest() {
    Directory executable, user;
    std::mutex metadata;
    auto settings = SettingsFor(executable.path, user.path, IdA);
    settings.game = [&metadata] {
        std::lock_guard guard(metadata);
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    std::unique_lock held(metadata);
    std::thread starter([&collector] { collector.Start(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto done = std::async(std::launch::async, [&collector] { collector.Finish(RunReport::Exit::Abort, {RunReport::Kind::Assert, RunReport::Reason::Crash, std::nullopt}, std::nullopt); });
    const bool returned = done.wait_for(std::chrono::seconds(8)) == std::future_status::ready;
    held.unlock();
    done.wait();
    starter.join();
    Require(returned, "Finish blocked while the game metadata lock was held");
    Require(Contains(Read(collector.ReportPath()), "\"kind\": \"assert\""), "the report of the assertion was not written");
}

void CleanFinishReadsGameOnceTest() {
    Directory executable, user;
    std::atomic<int> calls{0};
    auto settings = SettingsFor(executable.path, user.path, IdA);
    settings.game = [&calls] {
        ++calls;
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        game.name = "Example Game";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    Require(calls == 1 && Contains(Read(collector.ReportPath()), "\"titleId\": \"PPSA12345\""), "a clean finish did not read the game once");
}

void ParentLinkTest() {
#ifdef _WIN32
    throw NotRun("symbolic links need a privilege here");
#else
    Directory directory;
    fs::create_directories(directory.path / "real");
    fs::create_directory_symlink(directory.path / "real", directory.path / "link");
    std::string error;
    const bool wrote = RunReport::WriteAtomically(directory.path / "link" / "anyps5-report.json", "x\n", "aaaaaaaa", error);
    Require(!wrote && !fs::exists(directory.path / "real" / "anyps5-report.json"), "a write went through a symbolic link in the parent folder");
    Directory user, other;
    fs::create_directory_symlink(other.path, user.path / "reports");
    std::ofstream(directory.path / "file") << "not a directory";
    RunReport::Collector collector(SettingsFor(directory.path / "file" / "below", user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    Require(!fs::exists(other.path / IdA / "anyps5-report.json"), "a collector used the reports folder that was a link");
    std::size_t entries = 0;
    for (const auto& entry : fs::directory_iterator(other.path)) { static_cast<void>(entry); ++entries; }
    Require(entries == 0, "something was written through the planted reports link");
#endif
}

void PrivacyIsolatedTest() {
    RunReport::HostIdentifiers host;
    host.words = {"ab", "canaryuser", "canary-host-7"};
    for (const char* unsafe : {"User ab", "ab", "played by AB.", "connect 2001:db8::1", "ping fe80::1", "loopback ::1", "any ::", "mail alice@example.com", "read /opt/private/report",
                               "open /run/user/1000/x", "go to ~/Documents/saves", "account 123456789012", "see file:///etc/passwd", "set %USERPROFILE%\\x", "(/srv/data)", "http://example.org/x"}) {
        Require(!RunReport::SafeText(unsafe, host).has_value(), std::string("an unsafe text was kept: ") + unsafe);
    }
    for (const char* glued : {"lib_192.0.2.123", "fn192.0.2.123", "v192.0.2.123", "x_10.1.2.3_y"}) {
        Require(!RunReport::SafeText(glued, host).has_value(), std::string("an address glued to an identifier was kept: ") + glued);
    }
    for (const char* safe : {"Tabby and abacus", "Mission: Impossible: Redux", "Face Off: Dead", "Part 1/2", "Strike & Fire", "Game 2049", "Alpha: Omega"}) {
        Require(RunReport::SafeText(safe, host).has_value(), std::string("a safe text was dropped: ") + safe);
    }
}

void PrivacySweepTest() {
#ifdef _WIN32
    throw NotRun("environment canaries are POSIX here");
#else
    ::setenv("USER", "canaryuser9", 1);
    ::setenv("LOGNAME", "canaryuser9", 1);
    ::setenv("HOME", "/home/canaryuser9", 1);
    ::setenv("HOSTNAME", "canary-host-9", 1);
    Directory executable, user;
    auto settings = SettingsFor(executable.path, user.path, IdA);
    settings.game = [] {
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        game.contentVersion = "01.002.000";
        game.name = "Save of canaryuser9";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Abort, {RunReport::Kind::Other, RunReport::Reason::Crash, std::string("canary-host-9 at /home/canaryuser9/x")}, std::nullopt);
    const auto report = Read(collector.ReportPath());
    for (const char* canary : {"canaryuser9", "canary-host-9", "/home/", "@", "::"}) {
        Require(!Contains(report, canary), std::string("the output contains the canary ") + canary + ": " + report);
    }
    Require(Contains(report, "\"source\": null") && Contains(report, "\"name\": null"), "the private fields were not dropped: " + report);
#endif
}

RunReport::Settings Covered(const fs::path& executable, const fs::path& user, const char* id);

void PrivacySweepEverythingTest() {
#ifdef _WIN32
    throw NotRun("environment canaries are POSIX here");
#else
    ::setenv("USER", "canaryuser9", 1);
    ::setenv("HOME", "/home/canaryuser9", 1);
    ::setenv("HOSTNAME", "canary-host-9", 1);
    Directory executable, user;
    auto settings = Covered(executable.path, user.path, IdA);
    settings.game = [] {
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        game.contentVersion = "canaryuser9";
        game.name = "Game canary-host-9";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.NoteMissingFunction("libcanaryuser9", "fn_canaryuser9_call", "/home/canaryuser9/core/a.cpp");
    collector.NoteMissingFunction("lib/home/canaryuser9", "fn_ok", "C:\\Users\\canaryuser9\\core\\a.cpp");
    collector.NoteMissingFunction("lib_192.0.2.123", "account_123456789012", "");
    collector.NoteMissingFunction("libOk", "fn_ok", "");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto report = Read(collector.ReportPath());
    Require(!report.empty(), "the report is empty");
    for (const char* canary : {"canaryuser9", "canary-host-9", "/home/", "C:\\", "Users", "@", "192.0.2.123", "123456789012"}) {
        Require(!Contains(report, canary), std::string("the report contains the canary ") + canary + ": " + report);
    }
    Require(Contains(report, "\"fn_ok\""), "the clean entry next to the canaries was lost: " + report);
#endif
}

void StructuredFieldCanaryTest() {
    RunReport::HostIdentifiers host;
    host.words = {"canaryuser9"};
    auto report = SampleReport();
    report.game.contentVersion = "192.0.2.123";
    report.build.release = "v123456789012";
    auto& o = report.observed;
    o.coverage = {true, true, true, true, true, true};
    o.functions = {{std::string("lib_192.0.2.123"), "account_123456789012", 3}, {std::string("libOk"), "fn_ok", 2}, {std::string("libacct123456789012"), "fn_other", 1}};
    o.unsupported = {{std::string("VOP3"), std::string("acct123456789012"), 5, std::vector<std::string>{"acct123456789012"}, 4}, {std::string("VOP2"), std::string("OP_OK"), 6, std::vector<std::string>{"clamp"}, 2},
                     {std::string("acct123456789012"), std::string("OP_X"), 7, std::nullopt, 1}};
    o.imageFormats = {{1, std::string("acct123456789012"), RunReport::Status::Accepted, 0, 3}, {2, std::string("GuestFormat2"), RunReport::Status::Accepted, 0, 2}};
    const auto text = RunReport::Serialize(report, host);
    for (const char* canary : {"192.0.2.123", "123456789012", "canaryuser9"}) Require(!Contains(text, canary), std::string("a structured field kept the canary ") + canary + ": " + text);
    for (const char* kept : {"\"fn_ok\"", "\"libOk\"", "\"OP_OK\"", "\"clamp\"", "\"GuestFormat2\""}) Require(Contains(text, kept), std::string("a clean structured value was lost: ") + kept);
    Require(Contains(text, "\"contentVersion\": null") && Contains(text, "\"release\": null"), "a version or release with an address or account number was kept");
}

void EachStructuredFieldAloneTest() {
    RunReport::HostIdentifiers host;
    host.words = {"canaryuser9"};
    const auto base = [] {
        auto report = SampleReport();
        auto& o = report.observed;
        o.coverage = {true, true, true, true, true, true};
        o.functions = {{std::string("libOk"), "fn_ok", 3}, {std::string("libOk"), "fn_companion", 2}};
        o.unsupported = {{std::string("VOP3"), std::string("OP_OK"), 5, std::vector<std::string>{"clamp"}, 4}};
        o.imageFormats = {{1, std::string("GuestFormat1"), RunReport::Status::Accepted, 0, 3}};
        return report;
    };
    struct Case {
        const char* name;
        void (*apply)(RunReport::Report&, const std::string&);
        const char* kept;
    };
    const Case cases[] = {
        {"library", [](RunReport::Report& r, const std::string& v) { r.observed.functions[0].library = v; }, "\"fn_ok\""},
        {"function", [](RunReport::Report& r, const std::string& v) { r.observed.functions[0].name = v; }, "\"fn_companion\""},
        {"encoding", [](RunReport::Report& r, const std::string& v) { r.observed.unsupported[0].encoding = v; }, "\"OP_OK\""},
        {"opcode", [](RunReport::Report& r, const std::string& v) { r.observed.unsupported[0].opcode = v; }, "\"VOP3\""},
        {"modifier", [](RunReport::Report& r, const std::string& v) { r.observed.unsupported[0].modifiers = std::vector<std::string>{v}; }, "\"OP_OK\""},
        {"format name", [](RunReport::Report& r, const std::string& v) { r.observed.imageFormats[0].name = v; }, "\"format\": 1"},
        {"content version", [](RunReport::Report& r, const std::string& v) { r.game.contentVersion = v; }, "\"titleId\""},
        {"release", [](RunReport::Report& r, const std::string& v) { r.build.release = "v" + v; }, "\"commit\""},
    };
    for (const auto& unsafe : {std::string("192.0.2.123"), std::string("lib_192.0.2.123"), std::string("fn192.0.2.123"), std::string("OP_192.0.2.123"), std::string("acct123456789012"), std::string("canaryuser9")}) {
        for (const auto& item : cases) {
            auto report = base();
            item.apply(report, unsafe);
            const auto text = RunReport::Serialize(report, host);
            Require(!Contains(text, unsafe), std::string("the ") + item.name + " kept the unsafe value " + unsafe + ": " + text);
            Require(Contains(text, item.kept), std::string("a clean value was lost with the unsafe ") + item.name + ": " + text);
        }
    }
}
void SystemFieldsTest() {
    Directory executable, user;
    auto settings = Covered(executable.path, user.path, IdA);
    RunReport::SystemInfo machine;
    machine.osVersion = "Test Linux 1 (kernel 6.1.0)";
    machine.cpu = "Test CPU 9000 8-Core Processor";
    machine.ramGiB = 32;
    machine.gpu = RunReport::GpuInfo{std::string("NVIDIA"), std::string("Test GPU 4090"), std::string("NVIDIA 575.64.03"), std::string("1.3.280")};
    settings.machine = machine;
    {
        RunReport::Collector collector(std::move(settings));
        Require(collector.Start(), "the collector did not start");
        collector.Finish(RunReport::Exit::Clean, {}, 0);
        const auto text = Read(collector.ReportPath());
        for (const char* part : {"\"vendor\": \"NVIDIA\"", "\"name\": \"Test GPU 4090\"", "\"driver\": \"NVIDIA 575.64.03\"", "\"vulkan\": \"1.3.280\"", "\"cpu\": \"Test CPU 9000 8-Core Processor\""}) {
            Require(Contains(text, part), std::string("the report lacks ") + part + ": " + text);
        }
    }
    Directory other, userOther;
    auto hostile = Covered(other.path, userOther.path, IdB);
    RunReport::SystemInfo named;
    named.osVersion = "Linux 192.0.2.123 build";
    named.cpu = "CPU acct123456789012";
    named.gpu = RunReport::GpuInfo{std::string("Other"), std::string("GPU 10.1.2.3"), std::string("driver 1.0"), std::string("1.3.280")};
    hostile.machine = named;
    RunReport::Collector collector(std::move(hostile));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    Require(!Contains(text, "192.0.2.123") && !Contains(text, "123456789012") && !Contains(text, "10.1.2.3"), "a machine string kept an address or an account number: " + text);
    Require(Contains(text, "\"osVersion\": null") && Contains(text, "\"cpu\": null") && Contains(text, "\"driver\": \"driver 1.0\""), "a clean machine value was lost or an unsafe one kept: " + text);
}

RunReport::Report MachineReport(const RunReport::SystemInfo& machine) {
    auto report = SampleReport();
    report.machine = machine;
    return report;
}

RunReport::SystemInfo CleanMachine() {
    RunReport::SystemInfo machine;
    machine.osVersion = "Test Linux 1 (kernel 6.1.0)";
    machine.cpu = "Test CPU 9000 8-Core Processor";
    machine.ramGiB = 32;
    machine.gpu = RunReport::GpuInfo{std::string("NVIDIA"), std::string("Test GPU 4090"), std::string("NVIDIA 575.64.03"), std::string("1.3.280")};
    return machine;
}

void CpuClockSuffixTest() {
    RunReport::HostIdentifiers host;
    const auto cpuOf = [&](const char* cpu) {
        auto report = SampleReport();
        report.machine.cpu = std::string(cpu);
        return RunReport::Serialize(report, host);
    };
    const auto kept = [](const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; };
    auto text = cpuOf("Intel(R) Xeon(R) CPU E5-1650 v2 @ 3.50GHz");
    Require(kept(text, "\"cpu\": \"Intel(R) Xeon(R) CPU E5-1650 v2\"") && !kept(text, "GHz"), "the clock suffix was not dropped from the CPU name: " + text);
    text = cpuOf("VirtualApple @ 2.50GHz");
    Require(kept(text, "\"cpu\": \"VirtualApple\""), "the clock suffix was not dropped from the Rosetta CPU name: " + text);
    text = cpuOf("Apple M1 (Virtual)");
    Require(kept(text, "\"cpu\": \"Apple M1 (Virtual)\""), "a CPU name without a clock was changed: " + text);
    text = cpuOf("Core @ somebody@example.org");
    Require(kept(text, "\"cpu\": null"), "an address-like CPU name was kept: " + text);
    RunReport::HostIdentifiers named;
    named.words = {"alice"};
    named.hosts = {"alice"};
    named.users = {"alice"};
    for (const char* hostile : {"jdoe@3.5GHz", "ali@3.5GHz", "jdoe@3.5 GHz", "Core jdoe@3.5GHz", "jdoe @3.5GHz", "jdoe@ 3.5GHz", "alice@3.5GHz", "alice @ 3.5GHz"}) {
        auto report = SampleReport();
        report.machine.cpu = std::string(hostile);
        const auto output = RunReport::Serialize(report, named);
        Require(kept(output, "\"cpu\": null") && !kept(output, "jdoe") && !kept(output, "ali"), std::string("a CPU name with an address or a name before the clock was kept: ") + hostile + ": " + output);
    }
    {
        RunReport::HostIdentifiers whole;
        whole.words = {"alice @ 3.5GHz"};
        auto report = SampleReport();
        report.machine.cpu = std::string("alice @ 3.5GHz");
        const auto output = RunReport::Serialize(report, whole);
        Require(kept(output, "\"cpu\": null") && !kept(output, "alice"), "a host name that is the whole CPU text was kept after the clock was cut: " + output);
    }
    text = cpuOf("Core i7 @ 3.2GHz @ 3.5GHz extra");
    Require(kept(text, "\"cpu\": null"), "a CPU name with text after the clock was kept: " + text);
}

void MachineFieldCanaryTest() {
    RunReport::HostIdentifiers host;
    host.words = {"canaryuser9", "canary-host-9"};
    host.users = {"canaryuser9"};
    host.hosts = {"canary-host-9"};
    host.osIdentity = {"cachyos", "linux"};
    const char* canaries[] = {"canaryuser9", "canary-host-9", "a1b2c3d4-e5f6-4a3b-8c9d-aabbccddeeff", "/home/someone/x", "SN1234ABCD5678", "mail@example.com"};
    struct Field {
        const char* name;
        void (*apply)(RunReport::SystemInfo&, const std::string&);
        const char* kept;
    };
    const Field fields[] = {
        {"osVersion", [](RunReport::SystemInfo& m, const std::string& v) { m.osVersion = "Linux " + v + " (kernel 6.1.0)"; }, "\"Test CPU 9000 8-Core Processor\""},
        {"cpu", [](RunReport::SystemInfo& m, const std::string& v) { m.cpu = "CPU " + v; }, "\"Test Linux 1 (kernel 6.1.0)\""},
        {"gpu name", [](RunReport::SystemInfo& m, const std::string& v) { m.gpu->name = "GPU " + v; }, "\"NVIDIA 575.64.03\""},
        {"gpu driver", [](RunReport::SystemInfo& m, const std::string& v) { m.gpu->driver = "Driver " + v; }, "\"Test GPU 4090\""},
    };
    for (const std::string canary : canaries) {
        for (const auto& field : fields) {
            auto machine = CleanMachine();
            field.apply(machine, canary);
            const auto text = RunReport::Serialize(MachineReport(machine), host);
            Require(!Contains(text, canary), std::string("the ") + field.name + " kept the canary " + canary + ": " + text);
            Require(Contains(text, field.kept) && Contains(text, "\"ramGiB\": 32") && Contains(text, "\"vulkan\": \"1.3.280\""), std::string("a clean machine value was lost with the hostile ") + field.name + " " + canary + ": " + text);
        }
    }
}

void MachineHostNameRulesTest() {
    RunReport::SystemInfo machine = CleanMachine();
    machine.osVersion = "CachyOS Linux (kernel 7.2.3)";
    RunReport::HostIdentifiers sameAsDistro;
    sameAsDistro.words = {"cachyos"};
    sameAsDistro.hosts = {"cachyos"};
    sameAsDistro.osIdentity = {"cachyos", "linux", "arch"};
    Require(Contains(RunReport::Serialize(MachineReport(machine), sameAsDistro), "\"osVersion\": \"CachyOS Linux (kernel 7.2.3)\""), "a host named after its distribution lost the OS version");
    RunReport::HostIdentifiers laptop;
    laptop.words = {"bobs-laptop"};
    laptop.hosts = {"bobs-laptop"};
    laptop.osIdentity = {"cachyos", "linux"};
    machine.osVersion = "CachyOS Linux (kernel 6.8.0-bobs-laptop)";
    machine.cpu = "CPU of bobs-laptop";
    const auto otherText = RunReport::Serialize(MachineReport(machine), laptop);
    Require(!Contains(otherText, "bobs-laptop") && Contains(otherText, "\"osVersion\": null") && Contains(otherText, "\"cpu\": null"), "a host name outside the OS identity stayed in a machine string: " + otherText);
    RunReport::HostIdentifiers login;
    login.words = {"arch"};
    login.users = {"arch"};
    login.osIdentity = {"arch", "linux"};
    machine = CleanMachine();
    machine.osVersion = "Arch Linux (kernel 6.8.0)";
    Require(Contains(RunReport::Serialize(MachineReport(machine), login), "\"osVersion\": null"), "a login that equals a distribution word was compared with nothing");
    RunReport::HostIdentifiers both;
    both.words = {"arch"};
    both.users = {"arch"};
    both.hosts = {"arch"};
    both.osIdentity = {"arch"};
    Require(Contains(RunReport::Serialize(MachineReport(machine), both), "\"osVersion\": null"), "a name that is both login and host name got the host-name allowance");
}

void OsIdentityOnlyForTheOsVersionTest() {
    RunReport::HostIdentifiers bob;
    bob.words = {"bob", "alice"};
    bob.hosts = {"bob"};
    bob.users = {"alice"};
    bob.osIdentity = {"ubuntu"};
    auto machine = CleanMachine();
    machine.osVersion = "Ubuntu Bob Remix 24.04 (kernel 6.8.0)";
    machine.cpu = "CPU of bob";
    machine.gpu->name = "GPU of bob";
    machine.gpu->driver = "driver of bob";
    auto text = RunReport::Serialize(MachineReport(machine), bob);
    Require(!Contains(text, "Bob") && !Contains(text, "bob") && Contains(text, "\"osVersion\": null") && Contains(text, "\"cpu\": null") && Contains(text, "\"name\": null") && Contains(text, "\"driver\": null"), "a host name outside the OS identifier survived in a machine field: " + text);
    RunReport::HostIdentifiers cachy;
    cachy.words = {"cachyos"};
    cachy.hosts = {"cachyos"};
    cachy.osIdentity = {"cachyos"};
    machine = CleanMachine();
    machine.osVersion = "CachyOS Linux (kernel 7.2.3)";
    text = RunReport::Serialize(MachineReport(machine), cachy);
    Require(Contains(text, "\"osVersion\": \"CachyOS Linux (kernel 7.2.3)\""), "a host named after the OS identifier lost the OS version: " + text);
    machine.cpu = "Virtual CPU of cachyos";
    machine.gpu->name = "GPU cachyos";
    machine.gpu->driver = "driver cachyos";
    text = RunReport::Serialize(MachineReport(machine), cachy);
    Require(!Contains(text, "of cachyos") && !Contains(text, "GPU cachyos") && !Contains(text, "driver cachyos") && Contains(text, "\"cpu\": null") && Contains(text, "\"osVersion\": \"CachyOS Linux (kernel 7.2.3)\""), "the OS-identifier allowance reached another field: " + text);
    RunReport::HostIdentifiers compound;
    compound.words = {"ubuntu-bob"};
    compound.hosts = {"ubuntu-bob"};
    compound.osIdentity = {"ubuntu"};
    machine = CleanMachine();
    machine.osVersion = "Ubuntu 24.04 (kernel 6.8.0-ubuntu-bob)";
    text = RunReport::Serialize(MachineReport(machine), compound);
    Require(Contains(text, "\"osVersion\": \"Ubuntu 24.04 (kernel 6.8.0-ubuntu-bob)\"") == false, "a compound host name stayed in the OS version: " + text);
}

void OsReleaseIdentityTest() {
    std::istringstream codex("NAME=\"Ubuntu Bob Remix\"\nID=ubuntu\nID_LIKE=\"debian bob\"\nPRETTY_NAME=\"Ubuntu Bob Remix 24.04\"\n");
    const auto ids = RunReport::OsIdentityFromOsRelease(codex);
    Require(ids.size() == 1 && ids[0] == "ubuntu", "the OS identity is not the single listed ID value");
    std::istringstream quoted("ID=\"cachyos\"\n");
    Require(RunReport::OsIdentityFromOsRelease(quoted) == std::vector<std::string>{"cachyos"}, "a quoted public ID was not read");
    std::istringstream singleQuoted("ID='arch'\n");
    Require(RunReport::OsIdentityFromOsRelease(singleQuoted) == std::vector<std::string>{"arch"}, "a single-quoted public ID was not read");
    for (const char* text : {"ID=bob\n", "ID=BOB\n", "ID=\"bob\"\n", "ID= bob\n", "NAME=Bob Linux\n", "ID=\"bobX\n", "ID=Ubuntu\n", "ID=\"ubuntu\n", "ID='ubuntu\"\n", "ID=ubuntu bob\n", "ID=\"ubuntu bob\"\n"}) {
        std::istringstream in(text);
        Require(RunReport::OsIdentityFromOsRelease(in).empty(), std::string("a personal or malformed ID gave an identity: ") + text);
    }
    RunReport::HostIdentifiers host;
    host.words = {"bob", "alice"};
    host.hosts = {"bob"};
    host.users = {"alice"};
    for (const char* text : {"ID=bob\n", "ID=BOB\n", "ID=\"bob\"\n", "ID=\"bobX\n", "ID= bob\n", "NAME=\"Ubuntu Bob Remix\"\nID=ubuntu\nID_LIKE=\"debian bob\"\n"}) {
        std::istringstream in(text);
        host.osIdentity = RunReport::OsIdentityFromOsRelease(in);
        auto machine = CleanMachine();
        machine.osVersion = "Bob 24.04 (kernel 6.8.0)";
        machine.cpu = "Bob CPU";
        machine.gpu->name = "Bob GPU";
        machine.gpu->driver = "Bob driver";
        const auto out = RunReport::Serialize(MachineReport(machine), host);
        Require(!Contains(out, "Bob") && !Contains(out, "bob"), std::string("bob survived in a machine field with os-release ") + text + ": " + out);
    }
    host.osIdentity = {"bob"};
    auto machine = CleanMachine();
    machine.osVersion = "Bob 24.04 (kernel 6.8.0)";
    Require(Contains(RunReport::Serialize(MachineReport(machine), host), "\"osVersion\": null"), "an identity that is not on the public list exempted a host name");
}

void PublicHostNameTest() {
    for (const char* distro : {"cachyos", "ubuntu"}) {
        RunReport::HostIdentifiers host;
        host.words = {distro};
        host.hosts = {distro};
        std::istringstream in(std::string("ID=") + distro + "\n");
        host.osIdentity = RunReport::OsIdentityFromOsRelease(in);
        auto machine = CleanMachine();
        machine.osVersion = std::string(distro == std::string("cachyos") ? "CachyOS Linux" : "Ubuntu 24.04") + " (kernel 6.8.0)";
        machine.cpu = std::string("CPU of ") + distro;
        const auto out = RunReport::Serialize(MachineReport(machine), host);
        Require(Contains(out, distro == std::string("cachyos") ? "\"osVersion\": \"CachyOS Linux (kernel 6.8.0)\"" : "\"osVersion\": \"Ubuntu 24.04 (kernel 6.8.0)\""), std::string("a host named ") + distro + " on " + distro + " lost its OS version: " + out);
        Require(Contains(out, "\"cpu\": null"), std::string("the allowance reached the CPU for ") + distro + ": " + out);
    }
    RunReport::HostIdentifiers host;
    host.words = {"ubuntu"};
    host.hosts = {"ubuntu"};
    std::istringstream in("ID=bob\n");
    host.osIdentity = RunReport::OsIdentityFromOsRelease(in);
    auto machine = CleanMachine();
    machine.osVersion = "Ubuntu 24.04 (kernel 6.8.0)";
    Require(Contains(RunReport::Serialize(MachineReport(machine), host), "\"osVersion\": null"), "a host named ubuntu kept 'Ubuntu' while the os-release ID said bob");
    Require(RunReport::OsReleaseValue("\"CachyOS Linux\"") == std::optional<std::string>("CachyOS Linux"), "a quoted value was not read");
    Require(RunReport::OsReleaseValue("'Arch'") == std::optional<std::string>("Arch"), "a single-quoted value was not read");
    Require(RunReport::OsReleaseValue("rolling") == std::optional<std::string>("rolling"), "an unquoted value was not read");
    Require(!RunReport::OsReleaseValue("\"unmatched").has_value() && !RunReport::OsReleaseValue("'mixed\"").has_value() && !RunReport::OsReleaseValue(" leading").has_value() && !RunReport::OsReleaseValue("two words").has_value(), "a malformed value was accepted");
}

void OsVersionFormatTest() {
    Require(RunReport::FormatOsVersion("Arch Linux", "", "6.8.0-bob-custom") == "Arch Linux (kernel 6.8.0)", "the vendor suffix of the kernel release was kept: " + RunReport::FormatOsVersion("Arch Linux", "", "6.8.0-bob-custom"));
    Require(RunReport::FormatOsVersion("Ubuntu", "24.04", "5.15.153.1-microsoft-standard-WSL2") == "Ubuntu 24.04 (kernel 5.15.153)", "a WSL2 kernel release was not cut to three numbers");
    Require(RunReport::FormatOsVersion("", "", "6.1") == "kernel 6.1", "a two-number release without a name was wrong");
    Require(RunReport::FormatOsVersion("Fedora Linux", "40", "") == "Fedora Linux 40", "a missing kernel release broke the name");
    const RunReport::HostIdentifiers none;
    auto machine = CleanMachine();
    machine.osVersion = RunReport::FormatOsVersion("Ubuntu", "24.04", "5.15.153.1-microsoft-standard-WSL2");
    Require(Contains(RunReport::Serialize(MachineReport(machine), none), "\"osVersion\": \"Ubuntu 24.04 (kernel 5.15.153)\""), "a WSL2 host lost its OS version");
}

void HostProbeTest() {
#ifndef __linux__
    throw NotRun("the host probe is checked on Linux here");
#else
    const auto host = RunReport::ProbeHost();
    Require(host.ramGiB && *host.ramGiB >= 1, "the memory size of this host was not read");
    Require(host.osVersion && !host.osVersion->empty(), "the operating system version of this host was not read");
#if defined(__x86_64__) || defined(__i386__)
    Require(host.cpu && !host.cpu->empty(), "the CPU model of this host was not read");
#endif
    const auto current = RunReport::CurrentHostIdentifiers();
    const auto versionNames = RunReport::MachineNames(current, true);
    const auto machineNames = RunReport::MachineNames(current, false);
    if (host.osVersion) Require(RunReport::SafeText(*host.osVersion, versionNames).has_value(), "the OS version would be filtered out of the report: " + *host.osVersion);
    if (host.cpu) Require(RunReport::SafeText(*host.cpu, machineNames).has_value(), "the CPU model would be filtered out of the report: " + *host.cpu);
#endif
}

void NumericLocaleTest() {
#ifdef _WIN32
    throw NotRun("locale names differ");
#else
    const char* locales[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "de_DE", "fr_FR"};
    const std::string previous = std::setlocale(LC_NUMERIC, nullptr) ? std::setlocale(LC_NUMERIC, nullptr) : "C";
    bool changed = false;
    for (const char* name : locales) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr) {
            changed = true;
            break;
        }
    }
    if (!changed) throw NotRun("no comma-decimal locale is installed here");
    const auto text = RunReport::Serialize(SampleReport());
    std::setlocale(LC_NUMERIC, previous.c_str());
    Require(Contains(text, "\"seconds\": 12.500"), "the numeric locale changed the written number: " + text);
#endif
}


void AncestorLinkTest() {
#ifdef _WIN32
    throw NotRun("symbolic links need a privilege here");
#else
    Directory directory;
    fs::create_directories(directory.path / "real" / "sub");
    fs::create_directory_symlink(directory.path / "real", directory.path / "link");
    std::string error;
    const bool wrote = RunReport::WriteAtomically(directory.path / "link" / "sub" / "anyps5-report.json", "x\n", "aaaaaaaa", error);
    Require(!wrote && !fs::exists(directory.path / "real" / "sub" / "anyps5-report.json"), "a write went through a symbolic link above the parent folder");
    Require(RunReport::WriteAtomically(directory.path / "real" / "sub" / "anyps5-report.json", "x\n", "bbbbbbbb", error), "a write through real folders failed: " + error);
    std::string bytes;
    Require(!RunReport::ReadBounded(directory.path / "link" / "sub" / "anyps5-report.json", 100, bytes), "a read went through a symbolic link above the parent folder");
    Require(RunReport::ReadBounded(directory.path / "real" / "sub" / "anyps5-report.json", 100, bytes) && bytes == "x\n", "a read through real folders failed");
#endif
}

void RootLinkTest() {
#ifdef _WIN32
    throw NotRun("no root-owned links under the root here");
#else
    std::error_code error;
    for (const auto& entry : fs::directory_iterator("/", error)) {
        struct stat named {};
        if (::lstat(entry.path().c_str(), &named) != 0 || !S_ISLNK(named.st_mode) || named.st_uid != 0) continue;
        const fs::path target = fs::read_symlink(entry.path(), error);
        if (error || target.empty()) continue;
        bool plain = !target.relative_path().empty();
        fs::path prefix = "/";
        for (const auto& part : target.relative_path()) {
            if (part.empty()) continue;
            if (part == "." || part == "..") { plain = false; break; }
            prefix /= part;
            struct stat step {};
            if (::lstat(prefix.c_str(), &step) != 0 || !S_ISDIR(step.st_mode)) { plain = false; break; }
        }
        if (!plain) continue;
        Require(RunReport::FolderIsReachable(entry.path()), "a root-owned link under / was refused: " + entry.path().string());
        return;
    }
    throw NotRun("no root-owned link under / whose target is made of plain folders");
#endif
}

void MetadataHostTokenTest() {
    RunReport::HostIdentifiers host;
    host.words = {"ab", "canaryuser", "vcanaryrel"};
    auto report = SampleReport();
    report.game.contentVersion = "ab";
    report.build.release = "vcanaryrel";
    report.game.titleId = "PPSA12345";
    const auto text = RunReport::Serialize(report, host);
    Require(Contains(text, "\"contentVersion\": null") && Contains(text, "\"release\": null"), "a version or release that names the host was kept: " + text);
    Require(Contains(text, "\"titleId\": \"PPSA12345\""), "a harmless title id was dropped");
    report.build.commit = "abc1234";
    Require(Contains(RunReport::Serialize(report, host), "\"commit\": null"), "a commit that is not 40 hex digits was kept");
}

void PathDelimiterTest() {
    RunReport::HostIdentifiers host;
    for (const char* unsafe : {"file:/opt/x", "a:/opt/x", "a|/opt/x", "x;/srv/y"}) {
        Require(!RunReport::SafeText(unsafe, host).has_value(), std::string("an unsafe text was kept: ") + unsafe);
    }
}

void EarlyIdentityTest() {
    Directory executable, user;
    auto settings = SettingsFor(executable.path, user.path, IdA);
    settings.game = [] {
        RunReport::GameInfo game;
        game.titleId = "PPSA12345";
        game.name = "Example Game";
        return game;
    };
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Abort, {RunReport::Kind::Assert, RunReport::Reason::Crash, std::nullopt}, std::nullopt);
    Require(Contains(Read(collector.ReportPath()), "\"titleId\": \"PPSA12345\"") && Contains(Read(collector.ReportPath()), "\"name\": \"Example Game\""), "a run that failed early was reported without its game");
}

void ComponentProvenanceTest() {
    const std::string commit(40, 'a');
    const auto modifiedOf = [&](const std::string& name) {
        Directory executable, user;
        auto settings = SettingsFor(executable.path, user.path, IdA);
        settings.build.commit = commit;
        settings.build.modified = false;
        settings.requiredComponents = {name};
        RunReport::Collector collector(std::move(settings));
        Require(collector.Start(), "the collector did not start");
        collector.Finish(RunReport::Exit::Clean, {}, 0);
        const auto text = Read(collector.ReportPath());
        return Contains(text, "\"modified\": null") ? std::string("null") : Contains(text, "\"modified\": true") ? std::string("true") : std::string("false");
    };
    Require(modifiedOf("unregistered-lib") == "null", "a missing component did not make modified unknown");
    RunReport::RegisterComponent("same-lib", commit, 0);
    Require(modifiedOf("same-lib") == "false", "a matching clean component changed modified");
    RunReport::RegisterComponent("other-lib", std::string(40, 'b'), 0);
    Require(modifiedOf("other-lib") == "true", "a component from another commit did not make modified true");
    Require(modifiedOf("same-lib") == "true", "a registered library from another commit was ignored because it is not required");
    RunReport::RegisterComponent("dirty-lib", commit, 1);
    Require(modifiedOf("dirty-lib") == "true", "a modified component did not make modified true");
}

RunReport::Settings Covered(const fs::path& executable, const fs::path& user, const char* id) {
    auto settings = SettingsFor(executable, user, id);
    settings.coverage.missing = true;
    return settings;
}

void ListOrderAndCapTest() {
    Directory executable, user;
    RunReport::Collector collector(Covered(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    for (int index = 0; index < 300; ++index) {
        const auto name = "fn" + std::to_string(1000 + index);
        for (int call = 0; call <= index % 5; ++call) collector.NoteMissingFunction("libTest", name, "");
    }
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    std::size_t listed = 0;
    for (std::size_t at = text.find("\"calls\":"); at != std::string::npos; at = text.find("\"calls\":", at + 1)) ++listed;
    Require(listed == 200, "the function list was not capped at 200: " + std::to_string(listed));
    Require(Contains(text, "\"functions\": 100,\n    \"unsupported\": 0"), "the 100 dropped functions were not counted: " + text.substr(text.find("\"truncated\"")));
    Require(text.find("\"calls\": 5") < text.find("\"calls\": 1"), "the list is not ordered by count");
    Require(text.find("fn1004") < text.find("fn1009"), "equal counts are not ordered by name");
}

void SizeLimitTest() {
    auto report = SampleReport();
    auto& o = report.observed;
    o.coverage = {true, true, true, true, true, true};
    for (int index = 0; index < 300; ++index) {
        o.functions.push_back({std::string("libTestLibraryNameThatIsLong"), "aVeryLongFunctionNameThatTakesUpSpace" + std::string(80, 'x') + std::to_string(index), 1 + index % 9});
        o.unsupported.push_back({std::string("VOP3"), "V_LONG_OPCODE_NAME_" + std::string(40, 'Z') + std::to_string(index), index, std::nullopt, 1 + index % 9});
        o.imageFormats.push_back({index, "GuestFormatWithAVeryLongName" + std::string(30, 'q') + std::to_string(index), RunReport::Status::Accepted, index % 6, 1 + index % 9});
    }
    const auto text = RunReport::Serialize(report);
    Require(!text.empty() && text.size() <= RunReport::MaxBytes, "the report is over the size limit: " + std::to_string(text.size()));
    Require(Contains(text, "\"kind\": \"none\"") && Contains(text, "\"titleId\"") && Contains(text, "\"commit\""), "the stop or build details were cut");
    Require(!Contains(text, "\"functions\": 0,\n    \"unsupported\": 0"), "entries were cut without being counted");
}

void LostObservationTest() {
    Directory executable, user;
    auto settings = Covered(executable.path, user.path, IdA);
    settings.tableWait = std::chrono::milliseconds(20);
    RunReport::Collector collector(std::move(settings));
    Require(collector.Start(), "the collector did not start");
    collector.NoteMissingFunction("libTest", "first", "");
    {
        auto held = collector.LockTablesForTest();
        collector.NoteMissingFunction("libTest", "second", "");
    }
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    Require(Contains(text, "\"functions\": 1,\n    \"unsupported\": 0,\n    \"imageFormats\": 0,\n    \"descriptors\": 0"), "lost observations are not visible in truncated: " + text.substr(text.find("\"truncated\"")));
    Require(Contains(text, "\"incomplete\": true"), "a report that lost observations is not marked incomplete");
}

void BuildIdentitySurvivesTest() {
    RunReport::HostIdentifiers host;
    host.words = {"canaryuser9"};
    auto report = SampleReport();
    report.build.commit = std::string("0123456789abcdef0123456789abcdef01234567");
    report.runId = "0123456789abcdef0123456789abcdef";
    report.build.release = "v0.1.3";
    report.game.contentVersion = "01.002.000";
    auto text = RunReport::Serialize(report, host);
    for (const char* kept : {"\"commit\": \"0123456789abcdef0123456789abcdef01234567\"", "\"id\": \"0123456789abcdef0123456789abcdef\"", "\"release\": \"v0.1.3\"", "\"contentVersion\": \"01.002.000\""}) {
        Require(Contains(text, kept), std::string("build identity was lost: ") + kept + ": " + text);
    }
    report.game.contentVersion = "1.2.3.4";
    report.build.release = "v1.2.3.4";
    text = RunReport::Serialize(report, host);
    Require(Contains(text, "\"contentVersion\": null") && Contains(text, "\"release\": null") && Contains(text, "\"commit\": \"0123456789abcdef"), "a four-number version was kept, or the commit was lost with it: " + text);
}

void JunctionTest() {
#ifndef _WIN32
    throw NotRun("junctions are a Windows feature");
#else
    Directory directory;
    fs::create_directories(directory.path / "real" / "sub");
    const auto junction = directory.path / "link";
    const auto command = "mklink /J \"" + junction.string() + "\" \"" + (directory.path / "real").string() + "\" > NUL";
    Require(std::system(("cmd /c " + command).c_str()) == 0, "could not make a junction");
    std::string error;
    Require(!RunReport::WriteAtomically(junction / "sub" / "anyps5-report.json", "x\n", "aaaaaaaa", error) && !fs::exists(directory.path / "real" / "sub" / "anyps5-report.json"), "a write went through a junction above the parent folder");
    Require(!RunReport::WriteAtomically(junction / "anyps5-report.json", "x\n", "bbbbbbbb", error) && !fs::exists(directory.path / "real" / "anyps5-report.json"), "a write went through a junction parent");
    Require(RunReport::WriteAtomically(directory.path / "real" / "sub" / "anyps5-report.json", "x\n", "cccccccc", error), "a write through real folders failed: " + error);
    std::string bytes;
    Require(!RunReport::ReadBounded(junction / "sub" / "anyps5-report.json", 100, bytes), "a read went through a junction");
    Require(RunReport::ReadBounded(directory.path / "real" / "sub" / "anyps5-report.json", 100, bytes) && bytes == "x\n", "a read through real folders failed");
    std::system(("cmd /c rmdir \"" + junction.string() + "\" > NUL").c_str());
#endif
}

void CommitWithHexNameTest() {
    RunReport::HostIdentifiers host;
    host.words = {"abc", "dead"};
    auto report = SampleReport();
    report.build.commit = std::string("0123456789abcdef0123456789abcdef01234567");
    const auto text = RunReport::Serialize(report, host);
    Require(Contains(text, "\"commit\": \"0123456789abcdef0123456789abcdef01234567\""), "a commit was dropped because a hex-looking name occurs inside it: " + text);
    report.build.release = "vdead1";
    Require(Contains(RunReport::Serialize(report, host), "\"release\": null"), "a release that names the host was kept");
}

void SearchOnlyAncestorTest() {
#ifdef _WIN32
    throw NotRun("POSIX permissions");
#else
    Directory directory;
    const auto closed = directory.path / "closed";
    fs::create_directories(closed / "inner");
    fs::permissions(closed, fs::perms::owner_exec | fs::perms::owner_write, fs::perm_options::replace);
    std::string error;
    const bool wrote = RunReport::WriteAtomically(closed / "inner" / "anyps5-report.json", "x\n", "aaaaaaaa", error);
    fs::permissions(closed, fs::perms::owner_all, fs::perm_options::replace);
    Require(wrote, "a write through a search-only ancestor failed: " + error);
    Require(Read(closed / "inner" / "anyps5-report.json") == "x\n", "the file was not written");
#endif
}

void SourceBoundaryTest() {
    Require(!RunReport::SourceOf("/x/notcore/a.cpp", 3) && !RunReport::SourceOf("/x/mycore/a.cpp", 3), "a path that only ends in core/ was taken for the source tree");
    Require(RunReport::SourceOf("/x/core/a.cpp", 3) == std::optional<std::string>("core/a.cpp:3") && RunReport::SourceOf("core/a.cpp", 3) == std::optional<std::string>("core/a.cpp:3"), "a source path was refused");
    Require(RunReport::SourceOf("C:\\x\\core\\a.cpp", 3) == std::nullopt || RunReport::SourceOf("/x/core/b/core/a.cpp", 4) == std::optional<std::string>("core/a.cpp:4"), "the last core/ segment was not used");
}

void DroppedEntriesAreDistinctTest() {
    Directory executable, user;
    RunReport::Collector collector(Covered(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    for (int round = 0; round < 2; ++round) {
        for (int index = 0; index < 600; ++index) collector.NoteMissingFunction("libMany", "function" + std::to_string(1000 + index), "");
    }
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    Require(Contains(text, "\"functions\": 400,"), "truncated does not count the dropped entries once each: " + text.substr(text.find("\"truncated\"")));
}

void ObservationPrivacyTest() {
    Directory executable, user;
    RunReport::Collector collector(Covered(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    collector.NoteMissingFunction("lib/../bad", "ok_name", "");
    collector.NoteMissingFunction("libOk", "has space", "");
    collector.NoteMissingFunction("libOk", "/home/someone/f", "");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    auto report = SampleReport();
    report.observed.coverage = {true, true, true, true, true, true};
    report.observed.unsupported = {{std::string("VOP 3"), std::string("op code"), 5000000000LL, std::nullopt, 1}};
    report.observed.imageFormats = {{7, std::string("name with space"), RunReport::Status::Unknown, 99, 1}};
    for (const auto& text : {Read(collector.ReportPath()), RunReport::Serialize(report)}) {
        Require(!Contains(text, "has space") && !Contains(text, "/home/") && !Contains(text, "bad") && !Contains(text, "op code") && !Contains(text, "name with space"), "an invalid or private value was written: " + text);
    }
    const auto written = Read(collector.ReportPath());
    Require(Contains(written, "\"name\": \"ok_name\"") && Contains(written, "\"library\": null"), "the valid name was lost or its invalid library kept: " + written);
    Require(Contains(RunReport::Serialize(report), "\"usage\": \"other\""), "an out-of-range usage was not mapped to other");
}

void CoverageOffTest() {
    Directory executable, user;
    RunReport::Collector collector(SettingsFor(executable.path, user.path, IdA));
    Require(collector.Start(), "the collector did not start");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    const auto text = Read(collector.ReportPath());
    Require(Contains(text, "\"milestones\": false,\n    \"missing\": false,\n    \"shaders\": false") && !Contains(text, "cacheHits"), "a section the build does not collect is marked as covered: " + text);
}

#ifndef _WIN32
int RunChild(const fs::path& executable, const fs::path& directory, const std::string& mode, const std::string& arguments = "", const std::string& environment = "APS5_REPORT=1") {
    const auto command = "cd '" + directory.string() + "' && " + environment + " exec '" + executable.string() + "' --child " + mode + " " + arguments + " 2>/dev/null";
    return std::system(command.c_str());
}

fs::path Self;

fs::path CopySelf(const Directory& directory) {
    const auto copy = directory.path / "game";
    fs::copy_file(Self, copy);
    return copy;
}

std::string Report(const Directory& directory) {
    return Read(directory.path / "anyps5-report.json");
}

void ChildExitTest() {
    Directory directory;
    RunChild(CopySelf(directory), directory.path, "exit");
    const auto text = Report(directory);
    Require(Contains(text, "\"exit\": \"clean\"") && Contains(text, "\"exitCode\": 7") && Contains(text, "\"reason\": \"guest-exit\"") && Contains(text, "\"kind\": \"none\"") && Contains(text, "\"recovered\": false"), "exit() through the runtime did not write a clean report: " + text);
}

void ChildFlagTest() {
    Directory directory;
    const auto copy = CopySelf(directory);
    const int status = RunChild(copy, directory.path, "exit", "--aps5-report=off", "APS5_REPORT=1");
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 7, "the run with --aps5-report=off did not exit with its own status");
    Require(!fs::exists(directory.path / "anyps5-report.json"), "--aps5-report=off still wrote a report");
    RunChild(copy, directory.path, "exit", "--aps5-report=on", "APS5_REPORT=0");
    Require(fs::exists(directory.path / "anyps5-report.json"), "--aps5-report=on did not write a report");
}

void ChildMissingFunctionTest() {
    Directory directory;
    const int status = RunChild(CopySelf(directory), directory.path, "caught-missing-function");
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the run that caught an unimplemented function did not exit normally");
    const auto text = Report(directory);
    for (const char* part : {"\"kind\": \"none\"", "\"exit\": \"clean\"", "\"name\": \"sceNpCaught\"", "\"calls\": 1", "\"missing\": true"}) {
        Require(Contains(text, part), std::string("the report of the caught missing function lacks ") + part + ": " + text);
    }
}

void GuestRootIsNotFixedAtLoadTest() {
    Directory directory;
    const auto output = directory.path / "resolved.txt";
    RunChild(CopySelf(directory), directory.path, "guest-root", "'" + output.string() + "'", "unset APS5_REPORT &&");
    const auto resolved = Read(output);
    Require(!resolved.empty(), "the child did not resolve a path");
    Require(SamePath(fs::path(resolved).parent_path(), directory.path / "later"), "the guest root was fixed when the library loaded, not at the first path lookup: " + resolved);
}

void LibraryFromSourceTest() {
    Directory directory;
    RunChild(CopySelf(directory), directory.path, "library-path");
    const auto text = Report(directory);
    Require(Contains(text, "\"library\": \"libA\""), "the library is not the one in the core/libs/prx/ path: " + text);
    Require(!Contains(text, "libB") && !Contains(text, "/home/x"), "a deeper prx/ folder or a host path got into the report: " + text);
}


void Dump(const fs::path& directory) {
    fs::create_directories(directory);
    auto write = [&](const char* name, const RunReport::Report& report) {
        std::ofstream(directory / name, std::ios::binary) << RunReport::Serialize(report);
    };
    auto clean = SampleReport();
    write("clean.json", clean);
    auto quit = SampleReport();
    quit.stop.reason = RunReport::Reason::QuitRequested;
    write("quit.json", quit);
    auto assertion = SampleReport();
    assertion.exit = RunReport::Exit::Abort;
    assertion.exitCode.reset();
    assertion.stop = {RunReport::Kind::Assert, RunReport::Reason::Crash, std::nullopt};
    write("assert.json", assertion);
    auto terminate = assertion;
    terminate.stop.kind = RunReport::Kind::Other;
    write("terminate.json", terminate);
    auto recovered = SampleReport();
    recovered.exit = RunReport::Exit::Killed;
    recovered.exitCode.reset();
    recovered.recovered = recovered.incomplete = true;
    recovered.stop = {RunReport::Kind::Other, RunReport::Reason::UncleanUnknown, std::nullopt};
    write("recovered.json", recovered);
    auto unknown = SampleReport();
    unknown.build = {};
    unknown.game = {};
    unknown.os.reset();
    write("unknown.json", unknown);
    auto rich = SampleReport();
    rich.exit = RunReport::Exit::Abort;
    rich.exitCode = 134;
    rich.stop = {RunReport::Kind::UnsupportedShader, RunReport::Reason::RuntimeStop, std::string("core/shader/recompiler/Translator/src/Vop3.cpp:812"), std::string("Shader uses an unsupported VOP3 instruction (V_CVT_PK_U8_F32).")};
    auto& o = rich.observed;
    o.loaded = 1.2; o.started = 1.5; o.firstDraw = 40.8; o.firstPresent = 41.0;
    o.framesPresented = 600; o.averageFps = 59.9; o.fpsWindowSeconds = 10.0;
    o.shadersCompiled = 4090; o.shadersFailed = 1; o.cacheHits = 5120;
    o.coverage = {true, true, true, true, true, true};
    o.functions = {{std::string("libSceNpTrophy"), "sceNpTrophyCreateContext", 3}, {std::nullopt, "legacyCaller", 1}};
    o.unsupported = {{std::string("VOP3"), std::string("V_CVT_PK_U8_F32"), 256, std::vector<std::string>{"clamp"}, 150}, {std::nullopt, std::nullopt, 4000, std::nullopt, 2}};
    o.imageFormats = {{1, std::string("GuestFormat1"), RunReport::Status::Rejected, 0, 100}, {2, std::nullopt, RunReport::Status::Unknown, 5, 3}};
    o.descriptors = {{0, RunReport::Status::Accepted, 0, 900}, {6, RunReport::Status::Unknown, 3, 2}};
    auto zero = SampleReport();
    zero.observed.coverage = {true, true, true, true, true, true};
    zero.observed.loaded = 1.0; zero.observed.started = 1.2; zero.observed.firstDraw = 3.0;
    write("zero-present.json", zero);
    rich.machine.osVersion = "Test Linux 1 (kernel 6.1.0)";
    rich.machine.cpu = "Test CPU 9000 8-Core Processor";
    rich.machine.ramGiB = 32;
    rich.machine.gpu = RunReport::GpuInfo{std::string("NVIDIA"), std::string("Test GPU 4090"), std::string("NVIDIA 575.64.03"), std::string("1.3.280")};
    write("rich.json", rich);
    auto many = rich;
    many.observed.functions.clear();
    for (int index = 0; index < 400; ++index) many.observed.functions.push_back({std::string("libMany"), "function" + std::to_string(1000 + index), 1 + index % 7});
    write("truncated.json", many);
    Directory executable, userFolder;
    RunReport::Collector collector(Covered(executable.path, userFolder.path, IdA));
    collector.Start();
    collector.NoteMissingFunction("libSceNpTrophy", "sceNpTrophyCreateContext", "core/libs/prx/libSceNpTrophy/a.cpp:10");
    collector.Finish(RunReport::Exit::Clean, {}, 0);
    fs::copy_file(collector.ReportPath(), directory / "collector-clean.json", fs::copy_options::overwrite_existing);
}

#endif

}

int main(int argc, char** argv) {
    try {
#ifndef _WIN32
        const std::string first = argc > 1 ? argv[1] : "";
        if (first == "--child" && argc > 2) {
            const std::string mode = argv[2];
            if (mode == "exit") LibcExit_nid_no_patch(7);
            if (mode == "library-path") {
                LibcRunReportMissingFunction_nid_no_patch("fnX_nid_postfix", "/home/x/core/libs/prx/libA/sub/prx/libB/c.cpp", 5);
                LibcRunReportFinishExit_nid_no_patch(0);
                return 0;
            }
            if (mode == "guest-root" && argc > 3) {
                fs::create_directories("later");
                fs::current_path("later");
                std::ofstream(argv[3]) << ResolvePath_nid_no_patch("x.txt").string();
                return 0;
            }
            if (mode == "caught-missing-function") {
                try { NotImplemented_nid_no_patch("sceNpCaught_nid_postfix"); } catch (const std::exception&) {}
                LibcRunReportFinishExit_nid_no_patch(0);
                return 0;
            }
            std::raise(SIGKILL);
            return 3;
        }
        if (first == "--dump" && argc > 2) {
            Dump(argv[2]);
            return 0;
        }
        Self = fs::absolute(argv[0]);
#endif
        Run("toggle", ToggleTests);
        Run("privacy", PrivacyTests);
        Run("serialize", SerializeTests);
        Run("finish", FinishTests);
        Run("concurrent-finish", ConcurrentFinishTests);
        Run("fallback", FallbackTests);
        Run("atomic", AtomicTests);
        Run("canary", CanaryTests);
        Run("flag-from-temporary", FlagFromTemporaryTest);
        Run("finish-while-provider-blocked", FinishWhileProviderBlockedTest);
        Run("clean-finish-reads-game-once", CleanFinishReadsGameOnceTest);
        Run("parent-link", ParentLinkTest);
        Run("privacy-isolated", PrivacyIsolatedTest);
        Run("privacy-sweep", PrivacySweepTest);
        Run("privacy-sweep-everything", PrivacySweepEverythingTest);
        Run("structured-field-canary", StructuredFieldCanaryTest);
        Run("each-structured-field-alone", EachStructuredFieldAloneTest);
        Run("numeric-locale", NumericLocaleTest);
        Run("ancestor-link", AncestorLinkTest);
        Run("root-link", RootLinkTest);
        Run("metadata-host-token", MetadataHostTokenTest);
        Run("path-delimiters", PathDelimiterTest);
        Run("early-identity", EarlyIdentityTest);
        Run("component-provenance", ComponentProvenanceTest);
        Run("list-order-and-cap", ListOrderAndCapTest);
        Run("size-limit", SizeLimitTest);
        Run("observation-privacy", ObservationPrivacyTest);
        Run("coverage-off", CoverageOffTest);
        Run("lost-observation", LostObservationTest);
        Run("source-boundary", SourceBoundaryTest);
        Run("build-identity-survives", BuildIdentitySurvivesTest);
        Run("junction", JunctionTest);
        Run("system-fields", SystemFieldsTest);
        Run("host-probe", HostProbeTest);
        Run("machine-field-canary", MachineFieldCanaryTest);
        Run("cpu-clock-suffix", CpuClockSuffixTest);
        Run("machine-host-name-rules", MachineHostNameRulesTest);
        Run("os-version-format", OsVersionFormatTest);
        Run("os-identity-only-for-the-os-version", OsIdentityOnlyForTheOsVersionTest);
        Run("os-release-identity", OsReleaseIdentityTest);
        Run("public-host-name", PublicHostNameTest);
        Run("commit-with-hex-name", CommitWithHexNameTest);
        Run("search-only-ancestor", SearchOnlyAncestorTest);
        Run("dropped-entries-are-distinct", DroppedEntriesAreDistinctTest);
#ifndef _WIN32
        Run("child-exit", ChildExitTest);
        Run("child-flag", ChildFlagTest);
        Run("child-library-from-source", LibraryFromSourceTest);
        Run("child-guest-root-not-fixed-at-load", GuestRootIsNotFixedAtLoadTest);
        Run("child-missing-function", ChildMissingFunctionTest);
#endif
        std::printf("run report tests: %d passed, %d failed, %d not run\n", passed, failed, notRun);
        if (passed + failed + notRun == 0) {
            std::fputs("no test ran\n", stderr);
            return 1;
        }
        return failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

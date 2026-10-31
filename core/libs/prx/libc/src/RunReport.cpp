#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <regex>
#include <set>
#include <system_error>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#include <tuple>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/utsname.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#include <climits>
#include <fcntl.h>
#include <sys/file.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach-o/dyld.h>
#endif
#endif

#if __has_include("RunReportRevision.hpp")
#include "RunReportRevision.hpp"
#endif

#ifndef APS5_REPORT_COMMIT
#define APS5_REPORT_COMMIT ""
#endif
#ifndef APS5_REPORT_RELEASE
#define APS5_REPORT_RELEASE ""
#endif
#ifndef APS5_REPORT_MODIFIED
#define APS5_REPORT_MODIFIED (-1)
#endif

namespace fs = std::filesystem;

namespace RunReport {

namespace {

constexpr const char* ReportName = "anyps5-report.json";

const char* KindName(Kind kind) {
    switch (kind) {
        case Kind::None: return "none";
        case Kind::MissingFunction: return "missing-function";
        case Kind::UnsupportedShader: return "unsupported-shader";
        case Kind::UnsupportedFormat: return "unsupported-format";
        case Kind::Crash: return "crash";
        case Kind::Assert: return "assert";
        case Kind::Other: return "other";
    }
    return "other";
}

const char* ReasonName(Reason reason) {
    switch (reason) {
        case Reason::GuestExit: return "guest-exit";
        case Reason::QuitRequested: return "quit-requested";
        case Reason::RuntimeStop: return "runtime-stop";
        case Reason::Crash: return "crash";
        case Reason::UncleanUnknown: return "unclean-unknown";
        case Reason::Other: return "other";
    }
    return "other";
}

const char* ExitName(Exit exit) {
    switch (exit) {
        case Exit::Clean: return "clean";
        case Exit::Crash: return "crash";
        case Exit::Abort: return "abort";
        case Exit::Killed: return "killed";
    }
    return "abort";
}

const char* StopMessage(const Stop& stop) {
    switch (stop.reason) {
        case Reason::GuestExit: return "The game exited normally.";
        case Reason::QuitRequested: return "The window was closed.";
        case Reason::RuntimeStop: return "The runtime stopped the run on an error it reported.";
        case Reason::Crash: return stop.kind == Kind::Assert ? "A runtime assertion failed." : "The process ended through the abort handler.";
        case Reason::UncleanUnknown: return "The run ended without writing a report; this one was recovered from a checkpoint.";
        case Reason::Other: return "The run ended in a way the runtime did not classify.";
    }
    return "The run ended in a way the runtime did not classify.";
}

class JsonWriter {
public:
    void BeginObject(const char* key = nullptr) { Open(key, '{'); }
    void EndObject() { Close('}'); }
    void BeginArray(const char* key) { Open(key, '['); }
    void EndArray() { Close(']'); }
    void String(const char* key, const std::optional<std::string>& value) { Key(key); if (value) Quote(*value); else text += "null"; }
    void String(const char* key, std::string_view value) { Key(key); Quote(value); }
    void Bool(const char* key, bool value) { Key(key); text += value ? "true" : "false"; }
    void Bool(const char* key, const std::optional<bool>& value) { Key(key); text += value ? (*value ? "true" : "false") : "null"; }
    void Int(const char* key, const std::optional<std::int64_t>& value) { Key(key); text += value ? std::to_string(*value) : "null"; }
    void Number(const char* key, double value) {
        Key(key);
        char buffer[40];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), std::isfinite(value) && value >= 0.0 ? value : 0.0, std::chars_format::fixed, 3);
        text.append(buffer, result.ec == std::errc() ? result.ptr : buffer);
    }
    void Null(const char* key) { Key(key); text += "null"; }
    std::string Take() { text += '\n'; return std::move(text); }

private:
    void Key(const char* key) {
        if (!first.empty()) {
            if (!first.back()) text += ',';
            first.back() = false;
            text += '\n';
            text.append(first.size() * 2u, ' ');
        }
        if (key != nullptr) { Quote(key); text += ": "; }
    }
    void Open(const char* key, char bracket) {
        Key(key);
        text += bracket;
        first.push_back(true);
    }
    void Close(char bracket) {
        const bool wasEmpty = first.back();
        first.pop_back();
        if (!wasEmpty) {
            text += '\n';
            text.append(first.size() * 2u, ' ');
        }
        text += bracket;
    }
    void Quote(std::string_view value) {
        text += '"';
        for (const unsigned char character : value) {
            switch (character) {
                case '"': text += "\\\""; break;
                case '\\': text += "\\\\"; break;
                case '\n': text += "\\n"; break;
                case '\r': text += "\\r"; break;
                case '\t': text += "\\t"; break;
                default:
                    if (character < 0x20u || character == 0x7fu) {
                        char escape[8];
                        std::snprintf(escape, sizeof(escape), "\\u%04x", character);
                        text += escape;
                    } else {
                        text += static_cast<char>(character);
                    }
            }
        }
        text += '"';
    }

    std::string text;
    std::vector<bool> first;
};

bool ValidUtf8(std::string_view text, std::size_t& characters) {
    characters = 0;
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = lead < 0x80u ? 1u : (lead >> 5u) == 0x6u ? 2u : (lead >> 4u) == 0xeu ? 3u : (lead >> 3u) == 0x1eu ? 4u : 0u;
        if (length == 0u || index + length > text.size()) return false;
        std::uint32_t code = length == 1u ? lead : lead & (0x7fu >> length);
        for (std::size_t follow = 1; follow < length; ++follow) {
            const auto byte = static_cast<unsigned char>(text[index + follow]);
            if ((byte & 0xc0u) != 0x80u) return false;
            code = (code << 6u) | (byte & 0x3fu);
        }
        if ((length == 2u && code < 0x80u) || (length == 3u && code < 0x800u) || (length == 4u && (code < 0x10000u || code > 0x10ffffu)) || (code >= 0xd800u && code <= 0xdfffu)) return false;
        index += length;
        ++characters;
    }
    return true;
}

std::string Lower(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

bool Hex32(std::string_view text) {
    return text.size() == 32u && std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

}

namespace {

std::string CollapseSpaces(std::string_view text) {
    std::string result;
    bool space = true;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0') {
            if (!space) result += ' ';
            space = true;
        } else {
            result += c;
            space = false;
        }
    }
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::optional<std::string> CpuBrand() {
#if defined(__x86_64__) || defined(__i386__)
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(0x80000000u, &eax, &ebx, &ecx, &edx) && eax >= 0x80000004u) {
        char brand[49] = {};
        for (unsigned leaf = 0; leaf < 3u; ++leaf) {
            __get_cpuid(0x80000002u + leaf, &eax, &ebx, &ecx, &edx);
            std::memcpy(brand + leaf * 16u, &eax, 4);
            std::memcpy(brand + leaf * 16u + 4u, &ebx, 4);
            std::memcpy(brand + leaf * 16u + 8u, &ecx, 4);
            std::memcpy(brand + leaf * 16u + 12u, &edx, 4);
        }
        auto text = CollapseSpaces(brand);
        if (!text.empty()) return text;
    }
#endif
#ifdef __linux__
    std::ifstream cpuinfo("/proc/cpuinfo");
    for (std::string line; std::getline(cpuinfo, line);) {
        if (line.rfind("model name", 0) == 0 || line.rfind("Model", 0) == 0) {
            const auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto text = CollapseSpaces(line.substr(colon + 1u));
                if (!text.empty()) return text;
            }
        }
    }
#endif
    return std::nullopt;
}

std::int64_t WholeGiB(std::uint64_t bytes) {
    return static_cast<std::int64_t>((bytes + (1ull << 29u)) >> 30u);
}

}

SystemInfo ProbeHost() {
    SystemInfo info;
    info.cpu = CpuBrand();
#if defined(__linux__)
    std::string name, versionId;
    std::ifstream release("/etc/os-release");
    for (std::string line; std::getline(release, line);) {
        if (line.rfind("NAME=", 0) == 0) name = OsReleaseValue(line.substr(5u)).value_or("");
        else if (line.rfind("VERSION_ID=", 0) == 0) versionId = OsReleaseValue(line.substr(11u)).value_or("");
    }
    utsname kernel{};
    const std::string release_ = uname(&kernel) == 0 ? std::string(kernel.release) : std::string();
    if (const auto version = FormatOsVersion(name.empty() ? "Linux" : name, versionId, release_); !version.empty()) info.osVersion = version;
    struct sysinfo memory{};
    if (sysinfo(&memory) == 0) info.ramGiB = WholeGiB(static_cast<std::uint64_t>(memory.totalram) * memory.mem_unit);
#elif defined(__APPLE__)
    char buffer[256] = {};
    std::size_t size = sizeof(buffer);
    if (sysctlbyname("kern.osproductversion", buffer, &size, nullptr, 0) == 0) info.osVersion = "macOS " + CollapseSpaces(buffer);
    std::uint64_t memory = 0;
    size = sizeof(memory);
    if (sysctlbyname("hw.memsize", &memory, &size, nullptr, 0) == 0) info.ramGiB = WholeGiB(memory);
    size = sizeof(buffer);
    std::memset(buffer, 0, sizeof(buffer));
    if (!info.cpu && sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) info.cpu = CollapseSpaces(buffer);
#elif defined(_WIN32)
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        if (const auto function = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))) {
            OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            if (function(&version) == 0) info.osVersion = "Windows " + std::to_string(version.dwMajorVersion) + "." + std::to_string(version.dwMinorVersion) + " build " + std::to_string(version.dwBuildNumber);
        }
    }
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) info.ramGiB = WholeGiB(memory.ullTotalPhys);
#endif
    if (info.ramGiB && *info.ramGiB < 1) info.ramGiB.reset();
    return info;
}

std::string GenerateRunId() {
    std::random_device device;
    std::array<std::uint32_t, 4> words{};
    for (auto& word : words) word = device();
    char text[33];
    std::snprintf(text, sizeof(text), "%08x%08x%08x%08x", words[0], words[1], words[2], words[3]);
    return text;
}

std::string FormatUtc(std::chrono::system_clock::time_point time) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &seconds);
#else
    gmtime_r(&seconds, &parts);
#endif
    char text[24];
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &parts);
    return text;
}

namespace {

bool NamesHost(std::string_view text, const HostIdentifiers& host) {
    const auto lowered = Lower(text);
    for (const auto& word : host.words) {
        if (word.empty()) continue;
        const auto needle = Lower(word);
        if (needle.size() >= 3u) {
            if (lowered.find(needle) != std::string::npos) return true;
            continue;
        }
        for (std::size_t at = lowered.find(needle); at != std::string::npos; at = lowered.find(needle, at + 1u)) {
            const bool startsWord = at == 0u || !std::isalnum(static_cast<unsigned char>(lowered[at - 1u]));
            const bool endsWord = at + needle.size() >= lowered.size() || !std::isalnum(static_cast<unsigned char>(lowered[at + needle.size()]));
            if (startsWord && endsWord) return true;
        }
    }
    return false;
}

}

std::optional<std::string> SafeText(std::string_view text, const HostIdentifiers& host) {
    std::size_t characters = 0;
    if (text.empty() || !ValidUtf8(text, characters) || characters > MaxTextCharacters) return std::nullopt;
    if (std::ranges::any_of(text, [](unsigned char character) { return character < 0x20u || character == 0x7fu; })) return std::nullopt;
    if (NamesHost(text, host)) return std::nullopt;
    static const std::regex addresses(R"(((^|[^0-9])\d{1,3}(\.\d{1,3}){3}($|[^0-9]))|(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})|((^|[^0-9A-Za-z:])(([0-9A-Fa-f]{1,4})?:){2,7}[0-9A-Fa-f]{0,4}($|[^0-9A-Za-z:]))|(\d{8,})|([0-9A-Fa-f]{16,})|([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}))");
    static const std::regex paths(R"((^|[\s"'(=,;:|<>\[{])(/[^\s/]|~/|%[A-Za-z_]+%|[A-Za-z]:[\\/]|\\\\|file:))");
    const std::string owned(text);
    if (owned.find('@') != std::string::npos || owned.find("://") != std::string::npos) return std::nullopt;
    if (std::regex_search(owned, addresses) || std::regex_search(owned, paths)) return std::nullopt;
    return owned;
}

namespace {

std::string LowerAscii(std::string_view text) {
    std::string result(text);
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

std::vector<std::string> OsIdentityWords() {
    std::vector<std::string> words;
#if defined(__linux__)
    std::ifstream release("/etc/os-release");
    words = OsIdentityFromOsRelease(release);
#elif defined(__APPLE__)
    words.push_back("macos");
#elif defined(_WIN32)
    words.push_back("windows");
#endif
    return words;
}

}

std::optional<std::string> OsReleaseValue(std::string_view raw) {
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ' || raw.back() == '\t')) raw.remove_suffix(1u);
    if (raw.empty()) return std::string();
    const char quote = raw.front();
    if (quote == '"' || quote == '\'') {
        if (raw.size() < 2u || raw.back() != quote) return std::nullopt;
        std::string value;
        for (std::size_t index = 1; index + 1u < raw.size(); ++index) {
            const char c = raw[index];
            if (c == quote) return std::nullopt;
            if (c == '\\' && quote == '"' && index + 2u < raw.size() && std::string_view("\\\"$`").find(raw[index + 1u]) != std::string_view::npos) {
                value += raw[++index];
                continue;
            }
            value += c;
        }
        return value;
    }
    if (std::isspace(static_cast<unsigned char>(quote))) return std::nullopt;
    if (raw.find_first_of(" \t\"';") != std::string_view::npos) return std::nullopt;
    return std::string(raw);
}

bool PublicDistroId(std::string_view id) {
    static constexpr std::string_view known[] = {
        "alpine", "almalinux", "altlinux", "amzn", "antergos", "arch", "archarm", "artix", "bazzite", "bodhi", "centos", "chimera", "clear-linux-os",
        "cachyos", "debian", "deepin", "devuan", "elementary", "endeavouros", "fedora", "garuda", "gentoo", "guix", "kali", "kaos", "kdeneon",
        "linuxmint", "mageia", "manjaro", "mx", "neon", "nixos", "nobara", "ol", "opensuse", "opensuse-leap", "opensuse-tumbleweed", "parrot",
        "photon", "pop", "pureos", "raspbian", "rhel", "rocky", "slackware", "solus", "steamos", "tails", "ubuntu", "void", "zorin",
        "windows", "macos",
    };
    return std::ranges::find(known, id) != std::end(known);
}

std::vector<std::string> OsIdentityFromOsRelease(std::istream& stream) {
    std::vector<std::string> words;
    for (std::string line; std::getline(stream, line);) {
        if (line.rfind("ID=", 0) != 0) continue;
        if (const auto value = OsReleaseValue(line.substr(3u)); value && PublicDistroId(*value)) words.push_back(*value);
        break;
    }
    return words;
}

HostIdentifiers CurrentHostIdentifiers() {
    HostIdentifiers host;
    const auto add = [&](std::vector<std::string>& category, std::string value) {
        if (value.empty()) return;
        host.words.push_back(value);
        category.push_back(std::move(value));
    };
    for (const char* name : {"USER", "USERNAME", "LOGNAME", "HOME", "USERPROFILE"}) {
        if (const char* value = std::getenv(name); value != nullptr && *value != '\0') add(host.users, value);
    }
    for (const char* name : {"HOSTNAME", "COMPUTERNAME"}) {
        if (const char* value = std::getenv(name); value != nullptr && *value != '\0') add(host.hosts, value);
    }
#ifdef _WIN32
    wchar_t account[257] = {};
    DWORD accountSize = static_cast<DWORD>(std::size(account));
    if (GetUserNameW(account, &accountSize) && account[0] != L'\0') {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, account, -1, nullptr, 0, nullptr, nullptr);
        if (bytes > 1) {
            std::string utf8(static_cast<std::size_t>(bytes - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, account, -1, utf8.data(), bytes, nullptr, nullptr);
            add(host.users, std::move(utf8));
        }
    }
    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD computerSize = static_cast<DWORD>(std::size(computer));
    if (GetComputerNameW(computer, &computerSize) && computer[0] != L'\0') {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, computer, -1, nullptr, 0, nullptr, nullptr);
        if (bytes > 1) {
            std::string utf8(static_cast<std::size_t>(bytes - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, computer, -1, utf8.data(), bytes, nullptr, nullptr);
            add(host.hosts, std::move(utf8));
        }
    }
#else
    char buffer[1024];
    passwd entry{};
    passwd* found = nullptr;
    if (getpwuid_r(geteuid(), &entry, buffer, sizeof(buffer), &found) == 0 && found != nullptr) {
        if (entry.pw_name != nullptr && *entry.pw_name != '\0') add(host.users, entry.pw_name);
        if (entry.pw_dir != nullptr && *entry.pw_dir != '\0') add(host.users, entry.pw_dir);
    }
    char name[256] = {};
    if (gethostname(name, sizeof(name) - 1u) == 0 && name[0] != '\0') add(host.hosts, name);
#endif
    for (auto* category : {&host.users, &host.hosts}) {
        const auto count = category->size();
        for (std::size_t index = 0; index < count; ++index) {
            const auto leaf = fs::path((*category)[index]).filename().string();
            if (!leaf.empty() && leaf != (*category)[index]) add(*category, leaf);
        }
    }
    host.osIdentity = OsIdentityWords();
    return host;
}

HostIdentifiers MachineNames(const HostIdentifiers& host, bool forOsVersion) {
    HostIdentifiers names;
    for (const auto& word : host.words) {
        const auto lowered = LowerAscii(word);
        const auto in = [&](const std::vector<std::string>& list) {
            return std::ranges::any_of(list, [&](const std::string& other) { return LowerAscii(other) == lowered; });
        };
        if (forOsVersion && in(host.hosts) && !in(host.users) && std::ranges::any_of(host.osIdentity, [&](const std::string& id) { return PublicDistroId(id) && LowerAscii(id) == lowered; })) continue;
        names.words.push_back(word);
    }
    return names;
}

std::string FormatOsVersion(std::string_view name, std::string_view versionId, std::string_view kernelRelease) {
    std::string kernel;
    std::size_t groups = 0;
    for (std::size_t at = 0; at < kernelRelease.size() && groups < 3u;) {
        std::size_t end = at;
        while (end < kernelRelease.size() && std::isdigit(static_cast<unsigned char>(kernelRelease[end]))) ++end;
        if (end == at) break;
        if (!kernel.empty()) kernel += '.';
        kernel += kernelRelease.substr(at, end - at);
        ++groups;
        if (end < kernelRelease.size() && kernelRelease[end] == '.') at = end + 1u;
        else break;
    }
    std::string result = CollapseSpaces(name);
    const auto version = CollapseSpaces(versionId);
    if (!version.empty()) result += (result.empty() ? "" : " ") + version;
    if (!kernel.empty()) result += (result.empty() ? "kernel " : " (kernel ") + kernel + (result.empty() ? "" : ")");
    return result;
}

Toggle ResolveToggle(std::optional<std::string_view> flag, std::optional<std::string_view> environment) {
    Toggle toggle;
    const auto parse = [&](std::optional<std::string_view> value, std::string_view on, std::string_view off, const char* source) {
        if (!value) return Choice::Default;
        if (*value == on) return Choice::On;
        if (*value == off) return Choice::Off;
        toggle.warnings.push_back(std::string("ignoring the invalid ") + source + " value '" + std::string(*value) + "'");
        return Choice::Default;
    };
    Choice choice = parse(flag, "on", "off", "--aps5-report");
    if (choice == Choice::Default) choice = parse(environment, "1", "0", "APS5_REPORT");
    toggle.enabled = choice != Choice::Off;
    toggle.explicitlyOn = choice == Choice::On;
    return toggle;
}

std::optional<std::string> FlagFromArguments(const std::vector<std::string>& arguments) {
    constexpr std::string_view prefix = "--aps5-report=";
    std::optional<std::string> found;
    for (const auto& argument : arguments) {
        if (std::string_view(argument).substr(0, prefix.size()) == prefix) found = argument.substr(prefix.size());
    }
    return found;
}

std::vector<std::string> CommandLineArguments() {
    std::vector<std::string> arguments;
#if defined(_WIN32)
    int count = 0;
    if (LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count)) {
        for (int index = 0; index < count; ++index) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, nullptr, 0, nullptr, nullptr);
            if (bytes <= 1) continue;
            std::string utf8(static_cast<std::size_t>(bytes - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide[index], -1, utf8.data(), bytes, nullptr, nullptr);
            arguments.push_back(std::move(utf8));
        }
        LocalFree(wide);
    }
#elif defined(__APPLE__)
    char** argv = *_NSGetArgv();
    for (int index = 0; index < *_NSGetArgc(); ++index) arguments.emplace_back(argv[index]);
#else
    std::ifstream file("/proc/self/cmdline", std::ios::binary);
    std::string all((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    for (std::size_t begin = 0; begin < all.size();) {
        const auto end = all.find('\0', begin);
        arguments.push_back(all.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1u;
    }
#endif
    return arguments;
}

fs::path ExecutableDirectory() {
    std::error_code error;
#if defined(_WIN32)
    std::wstring path(32768u, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0u || length >= path.size()) return {};
    path.resize(length);
    return fs::path(path).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
    path.resize(std::strlen(path.c_str()));
    return fs::weakly_canonical(path, error).parent_path();
#else
    return fs::read_symlink("/proc/self/exe", error).parent_path();
#endif
}

fs::path UserStateDirectory() {
#if defined(_WIN32)
    if (const char* base = std::getenv("LOCALAPPDATA"); base != nullptr && *base != '\0') return fs::path(base) / "AnyPS5";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') return fs::path(home) / "Library" / "Application Support" / "AnyPS5";
#else
    if (const char* base = std::getenv("XDG_DATA_HOME"); base != nullptr && *base == '/') return fs::path(base) / "AnyPS5";
    if (const char* home = std::getenv("HOME"); home != nullptr && *home == '/') return fs::path(home) / ".local" / "share" / "AnyPS5";
#endif
    return {};
}

std::string Serialize(const Report& report) {
    return Serialize(report, CurrentHostIdentifiers());
}

namespace {

bool SerialLike(std::string_view text) {
    std::size_t run = 0;
    bool digit = false, letter = false;
    for (std::size_t index = 0; index <= text.size(); ++index) {
        const bool alnum = index < text.size() && std::isalnum(static_cast<unsigned char>(text[index]));
        if (alnum) {
            ++run;
            digit = digit || std::isdigit(static_cast<unsigned char>(text[index]));
            letter = letter || std::isalpha(static_cast<unsigned char>(text[index]));
        } else {
            if (run >= 12u && digit && letter) return true;
            run = 0;
            digit = letter = false;
        }
    }
    return false;
}

Report Sanitized(Report report, const HostIdentifiers& host) {
    const auto matches = [](const std::optional<std::string>& value, const char* pattern) {
        return value && std::regex_match(*value, std::regex(pattern));
    };
    if (!std::regex_match(report.generated, std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)"))) report.generated = "1970-01-01T00:00:00Z";
    if (!matches(report.build.commit, "[0-9a-f]{40}")) report.build.commit.reset();
    if (!matches(report.build.release, "v[0-9A-Za-z][0-9A-Za-z._-]{0,62}")) report.build.release.reset();
    if (!matches(report.game.titleId, "[A-Z]{4}[0-9]{5}")) report.game.titleId.reset();
    if (!matches(report.game.contentVersion, "[0-9A-Za-z._-]{1,32}")) report.game.contentVersion.reset();
    for (auto* value : {&report.build.release, &report.game.titleId, &report.game.contentVersion, &report.os}) {
        if (*value && !SafeText(**value, host).has_value()) value->reset();
    }
    if (report.game.name) report.game.name = SafeText(*report.game.name, host);
    if (report.stop.source) {
        report.stop.source = SafeText(*report.stop.source, host);
        if (report.stop.source && (report.stop.source->find("..") != std::string::npos || !std::regex_match(*report.stop.source, std::regex(R"([A-Za-z0-9_][A-Za-z0-9_./+-]{0,254}:[0-9]{1,9})")))) report.stop.source.reset();
    }
    if (report.stop.message) report.stop.message = SafeText(*report.stop.message, host);
    if (report.os && *report.os != "linux" && *report.os != "windows" && *report.os != "macos") report.os.reset();
    const auto versionNames = MachineNames(host, true);
    const auto machineNames = MachineNames(host, false);
    const auto text = [&](std::optional<std::string>& value, const HostIdentifiers& names) {
        if (value && (value->empty() || !SafeText(*value, names).has_value() || SerialLike(*value))) value.reset();
    };
    text(report.machine.osVersion, versionNames);
    if (report.machine.cpu && NamesHost(*report.machine.cpu, machineNames)) report.machine.cpu.reset();
    if (report.machine.cpu) {
        static const std::regex clock(R"((.*[^ @]) +@ +[0-9]{1,2}([.,][0-9]{1,3})? ?[GMgm][Hh][Zz])");
        std::smatch parts;
        if (std::regex_match(*report.machine.cpu, parts, clock) && parts[1].str().find('@') == std::string::npos) report.machine.cpu = parts[1].str();
    }
    text(report.machine.cpu, machineNames);
    if (report.machine.ramGiB && (*report.machine.ramGiB < 1 || *report.machine.ramGiB > 1048576)) report.machine.ramGiB.reset();
    if (report.machine.gpu) {
        auto& gpu = *report.machine.gpu;
        text(gpu.name, machineNames);
        text(gpu.driver, machineNames);
        if (gpu.vendor && *gpu.vendor != "AMD" && *gpu.vendor != "NVIDIA" && *gpu.vendor != "Intel" && *gpu.vendor != "Apple" && *gpu.vendor != "Other") gpu.vendor.reset();
        if (gpu.vulkan && !matches(gpu.vulkan, "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,5}")) gpu.vulkan.reset();
    }
    if (!Hex32(report.runId)) report.runId = std::string(32u, '0');
    return report;
}

}


namespace {

const char* StatusName(Status status) {
    return status == Status::Accepted ? "accepted" : status == Status::Rejected ? "rejected" : "unknown";
}

const char* FormatUsageName(int usage) {
    static constexpr const char* names[] = {"sampled", "storage", "color-target", "depth-target", "transfer", "other"};
    return names[usage >= 0 && usage < 6 ? usage : 5];
}

const char* DescriptorTypeName(int type) {
    static constexpr const char* names[] = {"UniformBuffer", "StorageBuffer", "UniformTexelBuffer", "StorageTexelBuffer", "SampledImage", "StorageImage", "Sampler"};
    return names[type >= 0 && type < 7 ? type : 0];
}

const char* DescriptorUsageName(int usage) {
    static constexpr const char* names[] = {"read", "write", "atomic", "other"};
    return names[usage >= 0 && usage < 4 ? usage : 3];
}

bool Matches(const std::string& text, const char* pattern) {
    return std::regex_match(text, std::regex(pattern));
}

std::int64_t CountOf(const FunctionEntry& entry) { return entry.calls; }
template <typename Entry> std::int64_t CountOf(const Entry& entry) { return entry.count; }

void NormalizeObservations(Observations& o, const HostIdentifiers& host) {
    const auto clean = [&](auto& list, std::int64_t& dropped, std::size_t cap, auto valid, auto order) {
        const auto before = list.size();
        std::erase_if(list, [&](auto& entry) { return CountOf(entry) < 1 || !valid(entry); });
        dropped += static_cast<std::int64_t>(before - list.size());
        std::stable_sort(list.begin(), list.end(), order);
        if (list.size() > cap) {
            dropped += static_cast<std::int64_t>(list.size() - cap);
            list.resize(cap);
        }
    };
    const auto optionalOk = [&](std::optional<std::string>& value, const char* pattern) {
        if (value && (!Matches(*value, pattern) || !SafeText(*value, host).has_value())) value.reset();
    };
    for (auto& entry : o.functions) optionalOk(entry.library, "[A-Za-z0-9_.-]{1,64}");
    clean(o.functions, o.truncated.functions, MaxListedFunctions,
          [&](const FunctionEntry& entry) { return Matches(entry.name, "[A-Za-z0-9_.$]{1,128}") && SafeText(entry.name, host).has_value(); },
          [](const FunctionEntry& a, const FunctionEntry& b) { return a.calls != b.calls ? a.calls > b.calls : std::tie(a.library, a.name) < std::tie(b.library, b.name); });
    for (auto& entry : o.unsupported) {
        optionalOk(entry.encoding, "[A-Za-z0-9_]{1,32}");
        optionalOk(entry.opcode, "[A-Za-z0-9_]{1,64}");
        if (entry.opcodeValue && (*entry.opcodeValue < 0 || *entry.opcodeValue > 4294967295LL)) entry.opcodeValue.reset();
        if (entry.modifiers) {
            bool ok = entry.modifiers->size() <= 16u;
            for (const auto& modifier : *entry.modifiers) ok = ok && Matches(modifier, "[a-z0-9][a-z0-9-]{0,31}") && SafeText(modifier, host).has_value();
            if (!ok) entry.modifiers.reset();
        }
    }
    clean(o.unsupported, o.truncated.unsupported, MaxListedFunctions, [](const UnsupportedEntry&) { return true; },
          [](const UnsupportedEntry& a, const UnsupportedEntry& b) {
              return a.count != b.count ? a.count > b.count : std::tie(a.encoding, a.opcode, a.opcodeValue) < std::tie(b.encoding, b.opcode, b.opcodeValue);
          });
    for (auto& entry : o.imageFormats) optionalOk(entry.name, "[A-Za-z0-9_]{1,64}");
    clean(o.imageFormats, o.truncated.imageFormats, MaxListedFormats, [](const FormatEntry& entry) { return entry.format >= 0 && entry.format <= 4294967295LL; },
          [](const FormatEntry& a, const FormatEntry& b) { return a.count != b.count ? a.count > b.count : std::tie(a.format, a.usage) < std::tie(b.format, b.usage); });
    clean(o.descriptors, o.truncated.descriptors, MaxListedFormats, [](const DescriptorEntry& entry) { return entry.type >= 0 && entry.type < 7; },
          [](const DescriptorEntry& a, const DescriptorEntry& b) { return a.count != b.count ? a.count > b.count : std::tie(a.type, a.usage, a.status) < std::tie(b.type, b.usage, b.status); });
    if (!o.coverage.milestones) {
        o.loaded = o.started = o.firstDraw = o.firstPresent = std::nullopt;
        o.framesPresented = 0;
        o.averageFps = o.fpsWindowSeconds = std::nullopt;
    }
    if (!o.coverage.missing) o.functions.clear(), o.truncated.functions = 0;
    if (!o.coverage.shaders) o.shadersCompiled = o.shadersFailed = 0, o.cacheHits.reset();
    if (!o.coverage.unsupported) o.unsupported.clear(), o.truncated.unsupported = 0;
    if (!o.coverage.imageFormats) o.imageFormats.clear(), o.truncated.imageFormats = 0;
    if (!o.coverage.descriptors) o.descriptors.clear(), o.truncated.descriptors = 0;
    if (o.averageFps && !(*o.averageFps >= 0.0 && *o.averageFps <= 1.0e6)) o.averageFps.reset();
    if (o.fpsWindowSeconds && !(*o.fpsWindowSeconds >= 0.001 && *o.fpsWindowSeconds <= 1.0e9)) o.fpsWindowSeconds.reset();
    if (o.framesPresented <= 1 || !o.fpsWindowSeconds) o.averageFps.reset(), o.fpsWindowSeconds.reset();
    if (o.framesPresented < 0) o.framesPresented = 0;
    for (auto* seconds : {&o.loaded, &o.started, &o.firstDraw, &o.firstPresent}) {
        if (*seconds && !(**seconds >= 0.0 && **seconds <= 1.0e9)) seconds->reset();
    }
    for (auto* counter : {&o.shadersCompiled, &o.shadersFailed}) if (*counter < 0) *counter = 0;
    if (o.cacheHits && *o.cacheHits < 0) o.cacheHits = 0;
}

std::string RenderReport(const Report& report) {
    JsonWriter json;
    json.BeginObject();
    json.String("format", std::string_view("anyps5-report"));
    json.Int("version", std::int64_t{1});
    json.String("generated", std::string_view(report.generated));
    json.BeginObject("anyps5");
    json.String("commit", report.build.commit);
    json.String("release", report.build.release);
    json.Bool("modified", report.build.modified);
    json.EndObject();
    json.BeginObject("game");
    json.String("titleId", report.game.titleId);
    json.Null("contentId");
    json.String("contentVersion", report.game.contentVersion);
    json.String("name", report.game.name);
    json.EndObject();
    json.BeginObject("system");
    json.String("os", report.os);
    json.String("osVersion", report.machine.osVersion);
    json.String("cpu", report.machine.cpu);
    json.Int("ramGiB", report.machine.ramGiB);
    if (report.machine.gpu) {
        json.BeginObject("gpu");
        json.String("vendor", report.machine.gpu->vendor);
        json.String("name", report.machine.gpu->name);
        json.String("driver", report.machine.gpu->driver);
        json.String("vulkan", report.machine.gpu->vulkan);
        json.EndObject();
    } else {
        json.Null("gpu");
    }
    json.EndObject();
    json.BeginObject("run");
    json.String("id", std::string_view(report.runId));
    json.Number("seconds", report.seconds);
    json.String("exit", std::string_view(ExitName(report.exit)));
    json.Int("exitCode", report.exitCode);
    const auto& o = report.observed;
    const char* stage = o.firstPresent ? "presents" : o.firstDraw ? "first-draw" : o.started ? "starts" : o.loaded ? "loads" : "unknown";
    json.String("stage", std::string_view(stage));
    json.BeginObject("milestones");
    if (o.loaded) json.Number("loaded", *o.loaded);
    if (o.started) json.Number("started", *o.started);
    if (o.firstDraw) json.Number("firstDraw", *o.firstDraw);
    if (o.firstPresent) json.Number("firstPresent", *o.firstPresent);
    json.EndObject();
    json.Int("framesPresented", o.framesPresented);
    if (o.averageFps) json.Number("averageFps", *o.averageFps); else json.Null("averageFps");
    if (o.fpsWindowSeconds) json.Number("fpsWindowSeconds", *o.fpsWindowSeconds); else json.Null("fpsWindowSeconds");
    json.Bool("recovered", report.recovered);
    json.Bool("incomplete", report.incomplete);
    json.EndObject();
    json.BeginObject("coverage");
    json.Bool("milestones", o.coverage.milestones);
    json.Bool("missing", o.coverage.missing);
    json.Bool("shaders", o.coverage.shaders);
    json.Bool("unsupported", o.coverage.unsupported);
    json.Bool("imageFormats", o.coverage.imageFormats);
    json.Bool("descriptors", o.coverage.descriptors);
    json.EndObject();
    json.BeginObject("truncated");
    json.Int("functions", o.truncated.functions);
    json.Int("unsupported", o.truncated.unsupported);
    json.Int("imageFormats", o.truncated.imageFormats);
    json.Int("descriptors", o.truncated.descriptors);
    json.EndObject();
    json.BeginObject("missing");
    json.BeginArray("functions");
    for (const auto& entry : o.functions) {
        json.BeginObject();
        json.String("library", entry.library);
        json.String("name", std::string_view(entry.name));
        json.Int("calls", entry.calls);
        json.EndObject();
    }
    json.EndArray();
    json.EndObject();
    json.BeginObject("shaders");
    json.Int("compiled", o.shadersCompiled);
    json.Int("failed", o.shadersFailed);
    if (o.cacheHits) json.Int("cacheHits", o.cacheHits);
    json.BeginArray("unsupported");
    for (const auto& entry : o.unsupported) {
        json.BeginObject();
        json.String("encoding", entry.encoding);
        json.String("opcode", entry.opcode);
        json.Int("opcodeValue", entry.opcodeValue);
        if (entry.modifiers) {
            json.BeginArray("modifiers");
            for (const auto& modifier : *entry.modifiers) json.String(nullptr, std::string_view(modifier));
            json.EndArray();
        } else {
            json.Null("modifiers");
        }
        json.Int("count", entry.count);
        json.EndObject();
    }
    json.EndArray();
    json.BeginArray("imageFormats");
    for (const auto& entry : o.imageFormats) {
        json.BeginObject();
        json.Int("format", entry.format);
        json.String("name", entry.name);
        json.String("status", std::string_view(StatusName(entry.status)));
        json.String("usage", std::string_view(FormatUsageName(entry.usage)));
        json.Int("count", entry.count);
        json.EndObject();
    }
    json.EndArray();
    json.BeginArray("descriptors");
    for (const auto& entry : o.descriptors) {
        json.BeginObject();
        json.String("type", std::string_view(DescriptorTypeName(entry.type)));
        json.String("status", std::string_view(StatusName(entry.status)));
        json.String("usage", std::string_view(DescriptorUsageName(entry.usage)));
        json.Int("count", entry.count);
        json.EndObject();
    }
    json.EndArray();
    json.EndObject();
    json.BeginObject("stop");
    json.String("kind", std::string_view(KindName(report.stop.kind)));
    json.String("reason", std::string_view(ReasonName(report.stop.reason)));
    json.String("message", report.stop.message ? std::string_view(*report.stop.message) : std::string_view(StopMessage(report.stop)));
    json.String("source", report.stop.source);
    json.EndObject();
    json.EndObject();
    return json.Take();
}

}

std::string Serialize(const Report& original, const HostIdentifiers& host) {
    Report report = Sanitized(original, host);
    NormalizeObservations(report.observed, host);
    for (;;) {
        auto text = RenderReport(report);
        if (text.size() <= MaxBytes) return text;
        auto& o = report.observed;
        const std::size_t sizes[4] = {o.functions.size(), o.unsupported.size(), o.imageFormats.size(), o.descriptors.size()};
        std::size_t longest = 0;
        for (std::size_t index = 1; index < 4; ++index) if (sizes[index] > sizes[longest]) longest = index;
        if (sizes[longest] == 0) throw std::runtime_error("run report: the serialized report exceeds 65,536 bytes");
        switch (longest) {
            case 0: o.functions.pop_back(); ++o.truncated.functions; break;
            case 1: o.unsupported.pop_back(); ++o.truncated.unsupported; break;
            case 2: o.imageFormats.pop_back(); ++o.truncated.imageFormats; break;
            default: o.descriptors.pop_back(); ++o.truncated.descriptors; break;
        }
    }
}

namespace {

#ifdef _WIN32
bool ReparseOrMissing(const fs::path& path, bool& reparse, bool& directory) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    reparse = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return true;
}

class PinnedChain {
public:
    PinnedChain() = default;
    PinnedChain(const PinnedChain&) = delete;
    PinnedChain& operator=(const PinnedChain&) = delete;
    ~PinnedChain() {
        for (const HANDLE handle : handles) CloseHandle(handle);
    }
    bool Pin(const fs::path& folder) {
        const auto absolute = fs::absolute(folder).lexically_normal();
        auto at = absolute.root_path();
        if (at.empty()) return false;
        std::vector<fs::path> chain{at};
        for (const auto& part : absolute.relative_path()) {
            if (part.empty() || part == L".") continue;
            at /= part;
            chain.push_back(at);
        }
        for (const auto& path : chain) {
            const HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                              FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (handle == INVALID_HANDLE_VALUE) return false;
            handles.push_back(handle);
            BY_HANDLE_FILE_INFORMATION information{};
            if (!GetFileInformationByHandle(handle, &information) || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return false;
        }
        return true;
    }

private:
    std::vector<HANDLE> handles;
};
#else
int OpenFolderNoFollow(const fs::path& folder) {
    const auto absolute = fs::absolute(folder).lexically_normal();
    std::vector<fs::path> parts;
    for (const auto& part : absolute.relative_path()) {
        if (!part.empty() && part != ".") parts.push_back(part);
    }
#if defined(O_PATH)
    constexpr int passThrough = O_PATH;
#elif defined(O_SEARCH)
    constexpr int passThrough = O_SEARCH;
#else
    constexpr int passThrough = O_RDONLY;
#endif
    int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int rootLinks = 0;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (current < 0) return -1;
        const bool last = index + 1u == parts.size();
        int next = ::openat(current, parts[index].c_str(), (last ? O_RDONLY : passThrough) | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && last && errno == EACCES) next = ::openat(current, parts[index].c_str(), passThrough | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && index == 0 && rootLinks < 8) {
            struct stat entry {};
            char target[PATH_MAX];
            const ssize_t length = ::fstatat(current, parts[0].c_str(), &entry, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(entry.st_mode) && entry.st_uid == 0
                    ? ::readlinkat(current, parts[0].c_str(), target, sizeof(target) - 1u) : -1;
            if (length > 0) {
                std::vector<fs::path> spliced;
                for (const auto& part : fs::path(std::string(target, static_cast<std::size_t>(length))).relative_path()) {
                    if (!part.empty() && part != ".") spliced.push_back(part);
                }
                spliced.insert(spliced.end(), parts.begin() + 1, parts.end());
                parts = std::move(spliced);
                ++rootLinks;
                index = static_cast<std::size_t>(-1);
                continue;
            }
        }
        ::close(current);
        current = next;
    }
    return current;
}
#endif

}

bool WriteAtomically(const fs::path& target, std::string_view bytes, std::string_view uniqueName, std::string& error) {
    const auto parent = target.parent_path().empty() ? fs::path(".") : target.parent_path();
    const auto leaf = target.filename().string();
    const auto temporaryName = "." + leaf + "." + std::string(uniqueName) + ".tmp";
#ifdef _WIN32
    bool reparse = false, directory = false;
    PinnedChain chain;
    if (!chain.Pin(parent)) {
        error = "the destination folder is missing, a link or not a folder";
        return false;
    }
    if (ReparseOrMissing(target, reparse, directory) && (reparse || directory)) {
        error = "the destination is a link or a folder";
        return false;
    }
    const auto temporary = parent / temporaryName;
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = "creating the temporary file failed";
        return false;
    }
    bool written = true;
    for (std::size_t done = 0; written && done < bytes.size();) {
        DWORD count = 0;
        written = WriteFile(file, bytes.data() + done, static_cast<DWORD>(std::min<std::size_t>(bytes.size() - done, 1u << 20u)), &count, nullptr) && count != 0u;
        done += count;
    }
    written = written && FlushFileBuffers(file);
    CloseHandle(file);
    if (!written) {
        DeleteFileW(temporary.c_str());
        error = "writing the temporary file failed";
        return false;
    }
    DWORD replaceError = 0;
    bool replaced = false;
    for (int attempt = 0; attempt < 20 && !replaced; ++attempt) {
        replaced = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (replaced) break;
        replaceError = GetLastError();
        if (replaceError != ERROR_ACCESS_DENIED && replaceError != ERROR_SHARING_VIOLATION && replaceError != ERROR_LOCK_VIOLATION) break;
        Sleep(5);
    }
    if (!replaced) {
        DeleteFileW(temporary.c_str());
        error = "replacing the destination failed (Windows error " + std::to_string(replaceError) + ")";
        return false;
    }
    return true;
#else
    const int folder = OpenFolderNoFollow(parent);
    if (folder < 0) {
        error = "the destination folder is missing, a link or not a folder";
        return false;
    }
    struct stat existing{};
    if (::fstatat(folder, leaf.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0 && !S_ISREG(existing.st_mode)) {
        ::close(folder);
        error = "the destination is a symbolic link or not a regular file";
        return false;
    }
    const int descriptor = ::openat(folder, temporaryName.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        ::close(folder);
        error = "creating the temporary file failed";
        return false;
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto count = ::write(descriptor, bytes.data() + done, bytes.size() - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        done += static_cast<std::size_t>(count);
    }
    bool durable = done == bytes.size();
#if defined(__APPLE__) && defined(F_FULLFSYNC)
    durable = durable && (::fcntl(descriptor, F_FULLFSYNC) == 0 || ::fsync(descriptor) == 0);
#else
    durable = durable && ::fsync(descriptor) == 0;
#endif
    ::close(descriptor);
    if (!durable) {
        ::unlinkat(folder, temporaryName.c_str(), 0);
        ::close(folder);
        error = "writing the temporary file failed";
        return false;
    }
    if (::renameat(folder, temporaryName.c_str(), folder, leaf.c_str()) != 0) {
        ::unlinkat(folder, temporaryName.c_str(), 0);
        ::close(folder);
        error = "replacing the destination failed";
        return false;
    }
    const bool flushed = ::fsync(folder) == 0 || errno == EINVAL || errno == ENOTSUP || errno == EBADF;
    ::close(folder);
    if (!flushed) error = "flushing the destination folder failed";
    return flushed;
#endif
}

bool FolderIsReachable(const fs::path& folder) {
#ifdef _WIN32
    PinnedChain chain;
    return chain.Pin(folder);
#else
    const int descriptor = OpenFolderNoFollow(folder);
    if (descriptor >= 0) ::close(descriptor);
    return descriptor >= 0;
#endif
}

bool ReadBounded(const fs::path& path, std::size_t limit, std::string& bytes) {
    bytes.clear();
#ifdef _WIN32
    PinnedChain chain;
    if (!chain.Pin(path.parent_path())) return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION information{};
    bool ok = GetFileInformationByHandle(file, &information) && !(information.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
    char buffer[4096];
    while (ok && bytes.size() <= limit) {
        DWORD count = 0;
        if (!ReadFile(file, buffer, static_cast<DWORD>(std::min<std::size_t>(sizeof(buffer), limit + 1u - bytes.size())), &count, nullptr)) { ok = false; break; }
        if (count == 0u) break;
        bytes.append(buffer, count);
    }
    CloseHandle(file);
    return ok && bytes.size() <= limit;
#else
    const int folder = OpenFolderNoFollow(path.parent_path().empty() ? fs::path(".") : path.parent_path());
    if (folder < 0) return false;
    const int descriptor = ::openat(folder, path.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    ::close(folder);
    if (descriptor < 0) return false;
    struct stat information{};
    bool ok = ::fstat(descriptor, &information) == 0 && S_ISREG(information.st_mode) && information.st_uid == geteuid();
    char buffer[4096];
    while (ok && bytes.size() <= limit) {
        const auto count = ::read(descriptor, buffer, std::min<std::size_t>(sizeof(buffer), limit + 1u - bytes.size()));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { ok = false; break; }
        if (count == 0) break;
        bytes.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(descriptor);
    return ok && bytes.size() <= limit;
#endif
}

namespace {

bool EnsureRealDirectory(const fs::path& path) {
    std::error_code error;
    auto status = fs::symlink_status(path, error);
    if (status.type() == fs::file_type::not_found) {
        fs::create_directory(path, error);
        status = fs::symlink_status(path, error);
    }
    return !error && status.type() == fs::file_type::directory;
}

fs::path Canonical(const fs::path& path) {
    std::error_code error;
    auto result = fs::weakly_canonical(path, error);
    return error ? path : result;
}

}

namespace {

struct Component {
    std::string name;
    std::string commit;
    int modified = -1;
};

std::mutex componentsMutex;

std::vector<Component>& Components() {
    static std::vector<Component> components;
    return components;
}

}

void RegisterComponent(std::string_view name, std::string_view commit, int modified) {
    std::lock_guard guard(componentsMutex);
    auto& components = Components();
    for (auto& component : components) {
        if (component.name == name) {
            component.commit = commit;
            component.modified = modified;
            return;
        }
    }
    components.push_back({std::string(name), std::string(commit), modified});
}

BuildInfo Collector::Provenance() const {
    BuildInfo build = settings.build;
    if (settings.requiredComponents.empty() || !build.modified) return build;
    std::lock_guard guard(componentsMutex);
    bool modified = *build.modified;
    bool unknown = false;
    for (const auto& component : Components()) {
        if (!build.commit || component.commit != *build.commit) modified = true;
        else if (component.modified < 0) unknown = true;
        else modified = modified || component.modified != 0;
    }
    for (const auto& name : settings.requiredComponents) {
        if (std::ranges::find(Components(), name, &Component::name) == Components().end()) unknown = true;
    }
    if (modified) build.modified = true;
    else if (unknown) build.modified.reset();
    else build.modified = false;
    return build;
}


struct Collector::GameState {
    std::mutex mutex;
    std::condition_variable done;
    GameInfo cache;
    bool known = false;
    int attempts = 0;
    int pending = 0;
};

constexpr std::size_t TrackedEntries = 512;

struct Collector::Tables {
    std::timed_mutex mutex;
    std::vector<FunctionEntry> functions;
    Truncated dropped;
    std::set<std::string> droppedFunctionKeys;
    bool droppedSaturated = false;
};

Collector::Collector(Settings value) : settings(std::move(value)), game(std::make_shared<GameState>()), tables(std::make_shared<Tables>()) {
    for (auto& count : lost) count = 0;
    if (!Hex32(settings.runId)) settings.runId = GenerateRunId();
    host = CurrentHostIdentifiers();
}

Collector::~Collector() = default;

bool Collector::Start() {
    started = std::chrono::steady_clock::now();
    auto probed = settings.machine ? *settings.machine : ProbeHost();
    {
        std::lock_guard guard(machineMutex);
        machineInfo = std::move(probed);
    }
    if (!settings.executableDirectory.empty()) {
        executableFolder = Canonical(settings.executableDirectory);
        candidates.push_back(executableFolder);
    }
    if (!settings.userStateDirectory.empty()) candidates.push_back(Canonical(settings.userStateDirectory) / "reports" / settings.runId);
    if (candidates.empty()) {
        std::fputs("[run-report] no folder for the run report; reporting is off for this run\n", stderr);
        return false;
    }
    reportPath = candidates.front() / ReportName;
    started_ = true;
    NoteGame();
    return true;
}

void Collector::NoteGame() {
    try {
        RefreshGame();
    } catch (...) {
    }
}

void Collector::RefreshGame(bool force) {
    const auto current = game;
    std::unique_lock guard(current->mutex);
    if (current->known || !settings.game) return;
    if (current->pending == 0) {
        if (!force && current->attempts >= 5) return;
        ++current->attempts;
        ++current->pending;
        std::thread([provider = settings.game, current] {
            GameInfo fields;
            try {
                fields = provider();
            } catch (...) {
            }
            std::lock_guard guard(current->mutex);
            if (fields.titleId) {
                current->cache = std::move(fields);
                current->known = true;
            }
            --current->pending;
            current->done.notify_all();
        }).detach();
    }
    current->done.wait_for(guard, std::chrono::seconds(1), [&current] { return current->pending == 0; });
}

Report Collector::Snapshot(Exit exit, Stop stop, std::optional<std::int64_t> exitCode) const {
    Report report;
    report.generated = FormatUtc(std::chrono::system_clock::now());
    report.build = Provenance();
    {
        std::lock_guard guard(machineMutex);
        report.machine = machineInfo;
    }
    {
        std::lock_guard guard(game->mutex);
        report.game = game->cache;
    }
    if (report.game.titleId) {
        static const std::regex titleId("^[A-Z]{4}[0-9]{5}$");
        if (!std::regex_match(*report.game.titleId, titleId)) report.game.titleId.reset();
    }
    if (report.game.contentVersion) {
        static const std::regex version("^[0-9A-Za-z._-]{1,32}$");
        if (!std::regex_match(*report.game.contentVersion, version)) report.game.contentVersion.reset();
    }
    if (report.game.name) report.game.name = SafeText(*report.game.name, host);
#if defined(_WIN32)
    report.os = "windows";
#elif defined(__APPLE__)
    report.os = "macos";
#else
    report.os = "linux";
#endif
    report.runId = settings.runId;
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    report.exit = exit;
    report.exitCode = exitCode;
    report.stop = std::move(stop);
    report.observed = Observe();
    if (report.observed.lostObservations) report.incomplete = true;
    return report;
}

std::optional<std::string> SourceOf(std::string_view file, int line) {
    auto at = file.rfind("core/");
    while (at != std::string_view::npos && at != 0u && file[at - 1u] != '/' && file[at - 1u] != '\\') at = at == 0u ? std::string_view::npos : file.rfind("core/", at - 1u);
    if (at == std::string_view::npos || line < 1) return std::nullopt;
    const auto path = file.substr(at);
    const auto digits = std::to_string(line);
    if (path.empty() || path.size() > 255u || digits.size() > 9u || path.find("..") != std::string_view::npos) return std::nullopt;
    const auto plain = [](char character) { return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_'; };
    if (!plain(path.front())) return std::nullopt;
    for (const char character : path) {
        if (!plain(character) && character != '.' && character != '/' && character != '+' && character != '-') return std::nullopt;
    }
    return std::string(path) + ":" + digits;
}

std::unique_lock<std::timed_mutex> Collector::LockTablesForTest() {
    return std::unique_lock<std::timed_mutex>(tables->mutex);
}

void Collector::NoteLost(int section) {
    lost[section].fetch_add(1, std::memory_order_relaxed);
}

void Collector::NoteMissingFunction(std::string_view library, std::string_view name, std::string_view) {
    if (!started_) return;
    std::unique_lock guard(tables->mutex, std::defer_lock);
    if (!guard.try_lock_for(settings.tableWait)) {
        NoteLost(0);
        return;
    }
    auto& list = tables->functions;
    const auto found = std::ranges::find_if(list, [&](const FunctionEntry& entry) {
        return entry.name == name && entry.library.value_or("") == library;
    });
    if (found != list.end()) {
        ++found->calls;
    } else if (list.size() < TrackedEntries) {
        list.push_back({library.empty() ? std::nullopt : std::optional<std::string>(library), std::string(name), 1});
    } else if (tables->droppedFunctionKeys.size() < 4096u) {
        if (tables->droppedFunctionKeys.insert(std::string(library) + "|" + std::string(name)).second) ++tables->dropped.functions;
    } else {
        tables->droppedSaturated = true;
    }
}

Observations Collector::Observe() const {
    Observations o;
    o.coverage = settings.coverage;
    std::unique_lock guard(tables->mutex, std::defer_lock);
    if (guard.try_lock_for(settings.tableWait)) {
        o.functions = tables->functions;
        o.truncated = tables->dropped;
        if (tables->droppedSaturated) o.lostObservations = true;
    } else {
        o.coverage.missing = false;
    }
    if (lost[0] != 0 && o.coverage.missing) o.truncated.functions += 1;
    o.lostObservations = o.lostObservations || lost[0] != 0;
    return o;
}

bool Collector::Finished() const noexcept {
    return state == 2;
}

void Collector::Finish(Exit exit, Stop stop, std::optional<std::int64_t> exitCode) {
    try {
        {
            std::unique_lock guard(mutex);
            if (!started_) return;
            if (state != 0) {
                changed.wait_for(guard, std::chrono::seconds(3), [this] { return state == 2; });
                return;
            }
            state = 1;
        }
        if (exit == Exit::Clean) RefreshGame(true);
        try {
            const auto text = Serialize(Snapshot(exit, std::move(stop), exitCode), host);
            std::string error;
            bool written = false;
            for (const auto& directory : candidates) {
                std::error_code created;
                if (directory == executableFolder) {
                    fs::create_directories(directory, created);
                    if (created) continue;
                } else {
                    fs::create_directories(settings.userStateDirectory, created);
                    if (created || !EnsureRealDirectory(directory.parent_path()) || !EnsureRealDirectory(directory)) continue;
                }
                const auto target = directory / ReportName;
                if (WriteAtomically(target, text, settings.runId.substr(0, 8), error)) {
                    reportPath = target;
                    written = true;
                    break;
                }
            }
            if (written) std::fprintf(stderr, "[run-report] wrote %s\n", reportPath.filename().string().c_str());
            else std::fprintf(stderr, "[run-report] could not write %s: %s\n", reportPath.filename().string().c_str(), error.c_str());
        } catch (const std::exception& failure) {
            std::fprintf(stderr, "[run-report] could not write the report: %s\n", failure.what());
        } catch (...) {
            std::fputs("[run-report] could not write the report\n", stderr);
        }
        std::lock_guard guard(mutex);
        state = 2;
        changed.notify_all();
    } catch (...) {
        state = 2;
    }
}

}


namespace {

std::atomic<bool (*)(RunReportGame*)> gameProvider{nullptr};
RunReport::Collector* collector = nullptr;

[[maybe_unused]] void FinishAtExit() {
    if (collector != nullptr && !collector->Finished()) collector->Finish(RunReport::Exit::Clean, RunReport::Stop{RunReport::Kind::None, RunReport::Reason::GuestExit, std::nullopt}, std::nullopt);
}

RunReport::GameInfo ProvideGame() {
    RunReport::GameInfo game;
    const auto provider = gameProvider.load(std::memory_order_acquire);
    RunReportGame fields{};
    if (provider == nullptr || !provider(&fields)) return game;
    fields.titleId[sizeof(fields.titleId) - 1u] = '\0';
    fields.contentVersion[sizeof(fields.contentVersion) - 1u] = '\0';
    fields.name[sizeof(fields.name) - 1u] = '\0';
    if (fields.titleId[0] != '\0') game.titleId = fields.titleId;
    if (fields.contentVersion[0] != '\0') game.contentVersion = fields.contentVersion;
    if (fields.name[0] != '\0') game.name = fields.name;
    return game;
}

bool GameProcess() {
    try {
        std::error_code error;
        const auto root = std::filesystem::current_path(error);
        if (error) return false;
        for (const char* name : {"app0/sce_sys/param.json", "app0/sce_sys/param.sfo"}) {
            if (std::filesystem::exists(root / name, error)) return true;
        }
    } catch (...) {
    }
    return false;
}

bool StartRunReport() noexcept {
    try {
        const char* environment = std::getenv("APS5_REPORT");
        const auto arguments = RunReport::CommandLineArguments();
        const auto flag = RunReport::FlagFromArguments(arguments);
        const auto toggle = RunReport::ResolveToggle(flag ? std::optional<std::string_view>(*flag) : std::nullopt, environment != nullptr ? std::optional<std::string_view>(environment) : std::nullopt);
        for (const auto& warning : toggle.warnings) std::fprintf(stderr, "[run-report] %s\n", warning.c_str());
        if (!toggle.enabled || (!toggle.explicitlyOn && !GameProcess())) return false;
        RunReport::Settings settings;
        settings.executableDirectory = RunReport::ExecutableDirectory();
        settings.userStateDirectory = RunReport::UserStateDirectory();
        settings.runId = RunReport::GenerateRunId();
        if (*APS5_REPORT_COMMIT != '\0') settings.build.commit = APS5_REPORT_COMMIT;
        if (*APS5_REPORT_RELEASE != '\0') settings.build.release = APS5_REPORT_RELEASE;
        if (APS5_REPORT_MODIFIED >= 0) settings.build.modified = APS5_REPORT_MODIFIED != 0;
        settings.game = ProvideGame;
        settings.requiredComponents = {"libkernel"};
        settings.coverage.missing = true;
        auto* created = new RunReport::Collector(std::move(settings));
        if (!created->Start()) {
            delete created;
            return false;
        }
        collector = created;
#ifndef _WIN32
        std::atexit(FinishAtExit);
#endif
        return true;
    } catch (...) {
        return false;
    }
}

[[maybe_unused]] const bool g_runReportStarted = StartRunReport();

std::string LibraryOf(std::string_view source) {
    constexpr std::string_view root = "core/libs/prx/";
    if (!source.starts_with(root)) return {};
    const auto rest = source.substr(root.size());
    const auto end = rest.find('/');
    return end == std::string_view::npos ? std::string() : std::string(rest.substr(0, end));
}

std::string_view ExportName(std::string_view name) {
    for (const std::string_view suffix : {"_nid_postfix", "_nid_no_patch"}) {
        if (name.ends_with(suffix)) return name.substr(0, name.size() - suffix.size());
    }
    return name;
}

}

extern "C" void LibcRunReportSetGameProvider_nid_no_patch(bool (*provider)(RunReportGame*)) {
    gameProvider.store(provider, std::memory_order_release);
    if (collector != nullptr) collector->NoteGame();
}

extern "C" void LibcRunReportRegisterComponent_nid_no_patch(const char* name, const char* commit, int modified) noexcept {
    try {
        if (name != nullptr) RunReport::RegisterComponent(name, commit != nullptr ? commit : "", modified);
    } catch (...) {
    }
}

extern "C" void LibcRunReportFinishExit_nid_no_patch(int exitCode) noexcept {
    if (collector == nullptr) return;
    collector->Finish(RunReport::Exit::Clean, RunReport::Stop{RunReport::Kind::None, RunReport::Reason::GuestExit, std::nullopt}, exitCode);
}

extern "C" void LibcRunReportMissingFunction_nid_no_patch(const char* name, const char* file, int line) noexcept {
    try {
        if (collector == nullptr || name == nullptr) return;
        const std::string_view path = file != nullptr ? file : "";
        const auto source = RunReport::SourceOf(path, line);
        collector->NoteMissingFunction(source ? LibraryOf(*source) : std::string(), ExportName(name), source.value_or(""));
    } catch (...) {
    }
}

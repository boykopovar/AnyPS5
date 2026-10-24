#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t SwitchCount = static_cast<std::size_t>(RuntimeSwitch::Count);

constexpr std::array<const char*, SwitchCount> SwitchNames{
    "APS5_NO_BARRIER_BATCH",
};

struct SwitchValues {
    std::array<std::atomic<bool>, SwitchCount> set{};
    SwitchValues() {
        const std::array<bool, SwitchCount> environment{
            std::getenv("APS5_NO_BARRIER_BATCH") != nullptr,
        };
        for (std::size_t index = 0; index < SwitchCount; ++index) set[index].store(environment[index], std::memory_order_relaxed);
    }
};

SwitchValues& Values() {
    static SwitchValues values;
    return values;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) text.remove_suffix(1);
    return text;
}

double ParseSeconds(std::string_view text, const char* what) {
    const std::string copy(Trim(text));
    char* end = nullptr;
    const double value = std::strtod(copy.c_str(), &end);
    if (copy.empty() || end != copy.c_str() + copy.size() || !std::isfinite(value) || value < 0) throw std::invalid_argument(std::string(what) + ": '" + copy + "' is not a number of seconds");
    return value;
}

std::string FlippableList() {
    std::string list;
    for (const auto* name : SwitchNames) list += std::string(list.empty() ? "" : ", ") + name;
    return list;
}

}

namespace RuntimeSwitches {

const char* Name(RuntimeSwitch id) {
    return SwitchNames.at(static_cast<std::size_t>(id));
}

bool Set(RuntimeSwitch id) {
    return Values().set.at(static_cast<std::size_t>(id)).load(std::memory_order_relaxed);
}

void Assign(RuntimeSwitch id, bool set) {
    Values().set.at(static_cast<std::size_t>(id)).store(set, std::memory_order_relaxed);
}

std::optional<RuntimeSwitch> Find(std::string_view name) {
    for (std::size_t index = 0; index < SwitchCount; ++index) {
        if (name == SwitchNames[index]) return static_cast<RuntimeSwitch>(index);
    }
    return std::nullopt;
}

}

AbSchedule::Config AbSchedule::Parse(std::string_view ab, std::string_view start, std::string_view settle) {
    Config config;
    ab = Trim(ab);
    const auto colon = ab.find(':');
    if (colon == std::string_view::npos) throw std::invalid_argument("APS5_AB: expected <seconds>:<switch>[,<switch>...], got '" + std::string(ab) + "'");
    config.phaseSeconds = ParseSeconds(ab.substr(0, colon), "APS5_AB phase length");
    if (config.phaseSeconds < 1) throw std::invalid_argument("APS5_AB: a phase must last at least 1 s");
    auto list = ab.substr(colon + 1);
    if (Trim(list).empty()) throw std::invalid_argument("APS5_AB: no switch listed after the phase length");
    for (;;) {
        const auto comma = list.find(',');
        const auto name = Trim(list.substr(0, comma));
        if (name.empty()) throw std::invalid_argument("APS5_AB: an empty switch name in '" + std::string(ab) + "'");
        const auto found = RuntimeSwitches::Find(name);
        if (!found.has_value()) throw std::invalid_argument("APS5_AB: " + std::string(name) + " cannot change while the title runs; switches the A/B can change: " + FlippableList());
        if (std::find(config.switches.begin(), config.switches.end(), *found) != config.switches.end()) throw std::invalid_argument("APS5_AB: " + std::string(name) + " is listed twice");
        config.switches.push_back(*found);
        if (comma == std::string_view::npos) break;
        list = list.substr(comma + 1);
    }
    start = Trim(start);
    if (!start.empty()) {
        if (start.back() == 's') {
            config.startSeconds = ParseSeconds(start.substr(0, start.size() - 1), "APS5_AB_START");
        } else {
            const std::string copy(start);
            char* end = nullptr;
            errno = 0;
            const auto value = std::strtoull(copy.c_str(), &end, 10);
            if (end != copy.c_str() + copy.size() || errno == ERANGE || copy.front() < '0' || copy.front() > '9') throw std::invalid_argument("APS5_AB_START: '" + copy + "' is neither a present index nor <n>s");
            config.startPresent = value;
        }
    }
    if (!Trim(settle).empty()) config.settleSeconds = ParseSeconds(settle, "APS5_AB_SETTLE");
    if (config.settleSeconds >= config.phaseSeconds) throw std::invalid_argument("APS5_AB_SETTLE must be shorter than the phase");
    return config;
}

AbSchedule::AbSchedule(Config config) : config(std::move(config)) {}

AbSchedule::Step AbSchedule::AtPresent(std::uint64_t present, double seconds) {
    if (!firstSeconds.has_value()) firstSeconds = seconds;
    if (!started) {
        const bool due = config.startSeconds.has_value() ? seconds - *firstSeconds >= *config.startSeconds : present >= config.startPresent;
        if (!due) return {};
        started = true;
        settled = false;
        phase = 0;
        phaseStart = seconds;
        return {Event::Begin, 0, false};
    }
    if (seconds - phaseStart >= config.phaseSeconds) {
        ++phase;
        settled = false;
        phaseStart = seconds;
        return {Event::Begin, phase, (phase & 1u) != 0};
    }
    if (!settled && seconds - phaseStart >= config.settleSeconds) {
        settled = true;
        return {Event::Settled, phase, (phase & 1u) != 0};
    }
    return {};
}

namespace {

struct AbState {
    std::mutex mutex;
    std::optional<AbSchedule> schedule;
    std::array<bool, SwitchCount> environment{};
};

AbState& Ab() {
    static AbState* state = [] {
        auto* created = new AbState();
        const char* ab = std::getenv("APS5_AB");
        const char* start = std::getenv("APS5_AB_START");
        const char* settle = std::getenv("APS5_AB_SETTLE");
        if (ab != nullptr) {
            created->schedule.emplace(AbSchedule::Parse(ab, start != nullptr ? start : "", settle != nullptr ? settle : ""));
            for (std::size_t index = 0; index < SwitchCount; ++index) created->environment[index] = RuntimeSwitches::Set(static_cast<RuntimeSwitch>(index));
        }
        return created;
    }();
    return *state;
}

std::string SwitchSettings(const AbState& state, bool b) {
    std::string text;
    for (const auto id : state.schedule->Settings().switches) {
        const bool set = state.environment[static_cast<std::size_t>(id)] != b;
        text += std::string(text.empty() ? "" : ", ") + RuntimeSwitches::Name(id) + (set ? " set" : " unset");
    }
    return text;
}

}

bool AbConfigured() {
    static const bool configured = std::getenv("APS5_AB") != nullptr;
    return configured;
}

std::vector<std::string> AbAtPresent(std::uint64_t present, double seconds) {
    if (!AbConfigured()) return {};
    auto& state = Ab();
    std::lock_guard lock(state.mutex);
    std::vector<std::string> lines;
    char text[512];
    const auto& config = state.schedule->Settings();
    const auto step = state.schedule->AtPresent(present, seconds);
    if (step.event == AbSchedule::Event::None) return lines;
    if (step.event == AbSchedule::Event::Begin) {
        if (step.phase == 0) {
            char from[96];
            if (config.startSeconds.has_value()) std::snprintf(from, sizeof(from), "%.1f s after the first present", *config.startSeconds);
            else std::snprintf(from, sizeof(from), "present %llu", static_cast<unsigned long long>(config.startPresent));
            std::snprintf(text, sizeof(text), "[ab] in-run A/B: phases of %.1f s from %s, settle %.1f s; A: %s; B: %s\n", config.phaseSeconds, from, config.settleSeconds, SwitchSettings(state, false).c_str(), SwitchSettings(state, true).c_str());
            lines.emplace_back(text);
        }
        for (const auto id : config.switches) RuntimeSwitches::Assign(id, state.environment[static_cast<std::size_t>(id)] != step.b);
        std::snprintf(text, sizeof(text), "[ab] phase %c %llu begins at present %llu (t %.3f s): %s\n", step.b ? 'B' : 'A', static_cast<unsigned long long>(step.phase), static_cast<unsigned long long>(present), seconds, SwitchSettings(state, step.b).c_str());
    } else {
        std::snprintf(text, sizeof(text), "[ab] phase %c %llu settled at present %llu (t %.3f s)\n", step.b ? 'B' : 'A', static_cast<unsigned long long>(step.phase), static_cast<unsigned long long>(present), seconds);
    }
    lines.emplace_back(text);
    return lines;
}

}

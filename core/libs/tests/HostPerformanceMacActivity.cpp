#include "prx/libkernel/System/include/MacActivity.hpp"
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace HostPerformance;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

struct Fixture {
    void* Handle = nullptr;
    const char* (*Events)() = nullptr;
    int (*PoolDepth)() = nullptr;
    int (*Pools)() = nullptr;

    explicit Fixture(const std::string& path) {
        Handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        Require(Handle != nullptr, "cannot load the fixture " + path);
        Events = reinterpret_cast<const char* (*)()>(dlsym(Handle, "fixture_events"));
        PoolDepth = reinterpret_cast<int (*)()>(dlsym(Handle, "fixture_pool_depth"));
        Pools = reinterpret_cast<int (*)()>(dlsym(Handle, "fixture_pools"));
        Require(Events != nullptr && PoolDepth != nullptr && Pools != nullptr, "the fixture lacks its accessors");
    }

    ~Fixture() { dlclose(Handle); }
};

std::string Expected(std::uint64_t options) {
    char text[32];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(options));
    return std::string("begin:") + text + ":AnyPS5 title running";
}

void OptionsAreUserInitiatedAndLatencyCritical() {
    Require(ActivityUserInitiated == 0x00FFFFFFULL, "NSActivityUserInitiated is 0x00FFFFFF, which includes the idle system sleep bit");
    Require(ActivityLatencyCritical == 0xFF00000000ULL, "NSActivityLatencyCritical is 0xFF00000000");
    Require((ActivityUserInitiated & ActivityLatencyCritical) == 0 && (ActivityUserInitiated & ActivityIdleSystemSleepDisabled) != 0, "the two option sets must not overlap");
}

void ActivityIsBegunRetainedAndEndedOnce(const std::string& path) {
    Fixture fixture(path);
    {
        MacActivity activity({path, path});
        Require(activity.Begin().Result == Outcome::Started, "a runtime that returns an activity must start");
        Require(activity.Begin().Result == Outcome::Started, "a repeated begin must keep the activity");
        const std::string expected = Expected(ActivityUserInitiated | ActivityLatencyCritical) + ",retain";
        Require(fixture.Events() == expected, std::string("begin must start one retained activity with both options, saw ") + fixture.Events());
        activity.End();
        activity.End();
        Require(fixture.Events() == expected + ",end,release", std::string("end must end and release once, saw ") + fixture.Events());
        Require(fixture.PoolDepth() == 0, "every autorelease pool must be popped");
    }
    Require(fixture.Pools() == 2, "begin and end must each use one autorelease pool");
}

void DestructorEndsAHeldActivity(const std::string& path) {
    Fixture fixture(path);
    const std::string before = fixture.Events();
    {
        MacActivity activity({path, path});
        activity.Begin();
    }
    Require(std::string(fixture.Events()).size() > before.size() && Contains(fixture.Events(), ",end,release"), "the destructor must end a held activity");
}

void MissingLibrariesAreUnavailable(const std::string& path, const std::string& missing) {
    MacActivity noRuntime({missing, path});
    const auto first = noRuntime.Begin();
    Require(first.Result == Outcome::Unavailable && Contains(first.Detail, missing), "a missing runtime library must be unavailable and named");
    MacActivity noFoundation({path, missing});
    const auto second = noFoundation.Begin();
    Require(second.Result == Outcome::Unavailable && Contains(second.Detail, missing), "a missing Foundation must be unavailable and named");
}

void MissingPoolFunctionsAreUnavailable(const std::string& path) {
    MacActivity activity({path, path});
    const auto begun = activity.Begin();
    Require(begun.Result == Outcome::Unavailable && Contains(begun.Detail, "autorelease pool"), "a runtime without pool functions must be unavailable");
}

void MissingClassIsUnavailableAndPoolBalanced(const std::string& path) {
    Fixture fixture(path);
    MacActivity activity({path, path});
    const auto begun = activity.Begin();
    Require(begun.Result == Outcome::Unavailable && Contains(begun.Detail, "NSProcessInfo"), "a runtime without NSProcessInfo must be unavailable");
    Require(fixture.PoolDepth() == 0, "a failed begin must still pop its pool");
    activity.End();
    Require(std::string(fixture.Events()).empty(), "nothing may be called on the activity that never began");
}

void NilActivityIsRefused(const std::string& path) {
    Fixture fixture(path);
    MacActivity activity({path, path});
    const auto begun = activity.Begin();
    Require(begun.Result == Outcome::Refused && Contains(begun.Detail, "beginActivityWithOptions"), "a nil activity must be refused");
    Require(fixture.PoolDepth() == 0, "a refused begin must still pop its pool");
    activity.End();
    Require(!Contains(fixture.Events(), "end"), "a refused activity must never be ended");
}

}

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: host_performance_mac_activity_tests <fixture> <fixture-no-class> <fixture-nil-activity> <fixture-no-pool> <missing-path>\n";
        return 2;
    }
    try {
        OptionsAreUserInitiatedAndLatencyCritical();
        ActivityIsBegunRetainedAndEndedOnce(argv[1]);
        DestructorEndsAHeldActivity(argv[1]);
        MissingLibrariesAreUnavailable(argv[1], argv[5]);
        MissingPoolFunctionsAreUnavailable(argv[4]);
        MissingClassIsUnavailableAndPoolBalanced(argv[2]);
        NilActivityIsRefused(argv[3]);
    } catch (const std::exception& error) {
        std::cerr << "host performance mac activity: " << error.what() << "\n";
        return 1;
    }
    std::cout << "host performance mac activity passed\n";
    return 0;
}

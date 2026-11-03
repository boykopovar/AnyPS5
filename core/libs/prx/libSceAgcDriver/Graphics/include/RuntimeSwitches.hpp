#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RUNTIMESWITCHES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RUNTIMESWITCHES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver::Graphics {

enum class RuntimeSwitch : std::size_t {
    NoBarrierBatch,
    Count,
};

namespace RuntimeSwitches {

const char* Name(RuntimeSwitch id);
bool Set(RuntimeSwitch id);
void Assign(RuntimeSwitch id, bool set);
std::optional<RuntimeSwitch> Find(std::string_view name);

}

class AbSchedule {
public:
    struct Config {
        double phaseSeconds = 0;
        double settleSeconds = 2;
        std::uint64_t startPresent = 0;
        std::optional<double> startSeconds;
        std::vector<RuntimeSwitch> switches;
    };

    enum class Event { None, Begin, Settled };

    struct Step {
        Event event = Event::None;
        std::uint64_t phase = 0;
        bool b = false;
    };

    static Config Parse(std::string_view ab, std::string_view start, std::string_view settle);

    explicit AbSchedule(Config config);

    Step AtPresent(std::uint64_t present, double seconds);

    const Config& Settings() const { return config; }

private:
    Config config;
    bool started = false;
    bool settled = false;
    std::uint64_t phase = 0;
    double phaseStart = 0;
    std::optional<double> firstSeconds;
};

bool AbConfigured();
std::vector<std::string> AbAtPresent(std::uint64_t present, double seconds);

}

#endif

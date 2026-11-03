#include "prx/libSceAgcDriver/Graphics/include/DrawStageTimingKeys.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <thread>

using Keys = AgcDriver::Graphics::DrawStageTimingKeys;

static auto Intern(Keys& keys, std::initializer_list<Keys::Stage> stages) {
    return keys.Intern(std::span<const Keys::Stage>(stages.begin(), stages.size()));
}

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        Keys keys;
        const auto first = Intern(keys, {{1, 11}, {5, 37}});
        Require(first.second, "first stage combination was not inserted");
        const auto reordered = Intern(keys, {{5, 37}, {1, 11}});
        Require(first.first == reordered.first && !reordered.second, "stage order changed the key");
        Require(Intern(keys, {{1, 12}, {5, 37}}).first != first.first, "shared fragment collapsed distinct vertex programs");
        Require(Intern(keys, {{1, 11}, {5, 38}}).first != first.first, "distinct fragment programs collapsed");
        Require(Intern(keys, {{1, 11}, {4, 2}, {5, 37}}).first != first.first, "geometry stage was ignored");
        Require(Intern(keys, {{1, 11}, {2, 2}, {3, 3}, {5, 37}}).first != first.first, "tessellation stages were ignored");
        const auto full = std::numeric_limits<std::uint64_t>::max();
        Require(Intern(keys, {{1, full}, {5, 37}}).first != Intern(keys, {{1, full >> 32u}, {5, 37}}).first,
                "variant ids lost their high bits");
        Require(Intern(keys, {}).first != Intern(keys, {{0, 0}}).first, "absent and zero-valued stages collapsed");
        Keys concurrent;
        std::array<std::uint64_t, 8> ids{};
        std::array<std::thread, 8> workers;
        std::atomic<bool> stable = true;
        for (std::size_t i = 0; i < workers.size(); ++i) {
            workers[i] = std::thread([&, i] {
                ids[i] = Intern(concurrent, {{1, 7}, {5, 37}}).first;
                for (unsigned n = 0; n < 100; ++n) {
                    if (Intern(concurrent, {{5, 37}, {1, 7}}).first != ids[i]) stable = false;
                }
            });
        }
        for (auto& worker : workers) worker.join();
        Require(stable && std::all_of(ids.begin(), ids.end(), [&](auto id) { return id == ids.front(); }),
                "concurrent interning assigned different keys");
        Keys bounded(1);
        const auto only = Intern(bounded, {{1, 1}}).first;
        bool rejected = false;
        try { Intern(bounded, {{1, 2}}); } catch (const std::overflow_error&) { rejected = true; }
        Require(rejected && Intern(bounded, {{1, 1}}).first == only, "capacity overflow wrapped or damaged a key");
        rejected = false;
        try { Keys invalid(Keys::MaxKey + 1); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected, "capacity allowed reserved timing bits");
        std::puts("draw-stage timing keys passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

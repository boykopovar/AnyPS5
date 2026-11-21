#include "prx/libSceVideoOut/include/MainThread.hpp"
#include "prx/libSceVideoOut/include/PadStatusSdl.hpp"
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using AgcDriver::PadKind;
using AgcDriver::PadPower;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

void typeTests() {
    constexpr std::array<std::pair<SDL_GameControllerType, PadKind>, SDL_CONTROLLER_TYPE_MAX> expected{{
        {SDL_CONTROLLER_TYPE_UNKNOWN, PadKind::Unknown},
        {SDL_CONTROLLER_TYPE_XBOX360, PadKind::Xbox},
        {SDL_CONTROLLER_TYPE_XBOXONE, PadKind::Xbox},
        {SDL_CONTROLLER_TYPE_PS3, PadKind::DualShock3},
        {SDL_CONTROLLER_TYPE_PS4, PadKind::DualShock4},
        {SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO, PadKind::Switch},
        {SDL_CONTROLLER_TYPE_VIRTUAL, PadKind::Unknown},
        {SDL_CONTROLLER_TYPE_PS5, PadKind::DualSense},
        {SDL_CONTROLLER_TYPE_AMAZON_LUNA, PadKind::Other},
        {SDL_CONTROLLER_TYPE_GOOGLE_STADIA, PadKind::Other},
        {SDL_CONTROLLER_TYPE_NVIDIA_SHIELD, PadKind::Other},
        {SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT, PadKind::Switch},
        {SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT, PadKind::Switch},
        {SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR, PadKind::Switch},
    }};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(static_cast<std::size_t>(expected[index].first) == index, "the SDL controller type table is out of order at " + std::to_string(index));
        require(PadKindFromSdl(expected[index].first) == expected[index].second, "SDL controller type " + std::to_string(index) + " maps to the wrong kind");
    }
    require(PadKindFromSdl(SDL_CONTROLLER_TYPE_MAX) == PadKind::Unknown, "the type bound is a kind");
    require(PadKindFromSdl(static_cast<SDL_GameControllerType>(SDL_CONTROLLER_TYPE_MAX + 1)) == PadKind::Unknown, "a type SDL does not define is a kind");
    std::printf("pad status sdl: every SDL controller type maps to its kind\n");
}

void powerTests() {
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_UNKNOWN) == PadPower::Unknown, "an unknown power level is a level");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_EMPTY) == PadPower::Empty, "empty maps wrongly");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_LOW) == PadPower::Low, "low maps wrongly");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_MEDIUM) == PadPower::Medium, "medium maps wrongly");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_FULL) == PadPower::Full, "full maps wrongly");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_WIRED) == PadPower::Wired, "wired maps wrongly");
    require(PadPowerFromSdl(SDL_JOYSTICK_POWER_MAX) == PadPower::Unknown, "the power bound is a level");
    require(PadPowerFromSdl(static_cast<SDL_JoystickPowerLevel>(SDL_JOYSTICK_POWER_MAX + 1)) == PadPower::Unknown, "a power level SDL does not define is a level");
    std::printf("pad status sdl: every SDL power level maps to its level\n");
}

void mainThreadTests() {
    auto syncRunner = [](void (*fn)(void*), void* ctx) { fn(ctx); };

    int value = 7;
    int& ref = OnMainThreadUsing([&value]() -> int& { return value; }, syncRunner);
    require(&ref == &value, "OnMainThreadUsing must preserve lvalue reference identity");
    ref = 42;
    require(value == 42, "mutating returned lvalue reference must mutate original target");

    static_assert(std::is_same_v<decltype(OnMainThreadUsing([]() -> const std::string { return "ok"; }, syncRunner)), const std::string>);
    const std::string constValue = OnMainThreadUsing([]() -> const std::string { return "ok"; }, syncRunner);
    require(constValue == "ok", "OnMainThreadUsing must return const values");

    auto ptr = OnMainThreadUsing([]() { return std::make_unique<int>(123); }, syncRunner);
    require(ptr && *ptr == 123, "OnMainThreadUsing must return move-only values");

    struct RvalueCallable {
        int result;
        int operator()() && {
            return result;
        }
    };
    require(OnMainThreadUsing(RvalueCallable{321}, syncRunner) == 321, "OnMainThreadUsing must forward rvalue-only callables");

    bool voidRan = false;
    OnMainThreadUsing([&voidRan]() { voidRan = true; }, syncRunner);
    require(voidRan, "OnMainThreadUsing must execute void callables");

    bool caught = false;
    try {
        OnMainThreadUsing([]() -> int { throw std::runtime_error("boom"); }, syncRunner);
    } catch (const std::runtime_error& error) {
        caught = std::string(error.what()) == "boom";
    }
    require(caught, "OnMainThreadUsing must rethrow exceptions from the callable");

    bool noopThrew = false;
    try {
        OnMainThreadUsing([]() { return 5; }, [](void (*)(void*), void*) {});
    } catch (const std::runtime_error&) {
        noopThrew = true;
    }
    require(noopThrew, "OnMainThreadUsing must throw when the runner does not invoke the callback");

    std::printf("pad status sdl: main thread dispatch handles references, cv-qualified values, and rvalues\n");
}

}

int main() try {
    typeTests();
    powerTests();
    mainThreadTests();
    std::printf("pad status sdl tests passed\n");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

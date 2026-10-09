#include "InputConfiguration.hpp"
#include "SDL_keyboard.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

void Require(bool value) {
    if (!value) throw std::runtime_error("input configuration check failed");
}

template<class TOperation> void Reject(TOperation operation) {
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-input-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        const auto path = directory / "input.ini";
        const auto defaults = InputConfig::LoadKeyboard(path);
        Require(defaults.size() == Pad::InputMapping.size());
        Reject([&] { InputConfig::LoadKeyboard(path, true); });
        InputConfig::WriteAtomic(path, "\xEF\xBB\xBF" "cross = KEY:F # override\nCross=KEY:F\nCross=KEY:Space\nR2=MOUSE:Right\n");
        const auto loaded = InputConfig::LoadKeyboard(path);
        int cross = 0;
        for (const auto& binding : loaded) if (binding.button == Pad::PadButton::Cross) ++cross;
        Require(cross == 2);
        const auto serialized = InputConfig::SerializeKeyboard(loaded);
        InputConfig::WriteAtomic(path, serialized);
        Require(InputConfig::SerializeKeyboard(InputConfig::LoadKeyboard(path)) == serialized);
        std::vector<Pad::InputBinding> allKeys;
        for (int code = 1; code < SDL_NUM_SCANCODES; ++code) {
            const auto scancode = static_cast<SDL_Scancode>(code);
            if (SDL_GetScancodeName(scancode)[0] != '\0') allKeys.push_back({scancode, Pad::MouseButton::None, Pad::InputControl::Button, Pad::PadButton::Cross});
        }
        const auto allKeyText = InputConfig::SerializeKeyboard(allKeys);
        InputConfig::WriteAtomic(path, allKeyText);
        Require(InputConfig::SerializeKeyboard(InputConfig::LoadKeyboard(path)) == allKeyText);
        InputConfig::WriteAtomic(path, "Cross=SCANCODE:-1\n");
        Reject([&] { InputConfig::LoadKeyboard(path); });
        InputConfig::WriteAtomic(path, "Cross=KEY:F\nCross=NONE\n");
        for (const auto& binding : InputConfig::LoadKeyboard(path)) Require(binding.button != Pad::PadButton::Cross);
        InputConfig::WriteAtomic(path, "ToggleFullscreen=MOUSE:Left\n");
        Reject([&] { InputConfig::LoadKeyboard(path); });
        InputConfig::WriteAtomic(path, "LeftStickLeft=WHEEL:Up\n");
        Reject([&] { InputConfig::LoadKeyboard(path); });
        InputConfig::WriteAtomic(path, serialized);
        Reject([&] { InputConfig::WriteAtomic(directory, "cannot replace directory"); });
        Require(InputConfig::SerializeKeyboard(InputConfig::LoadKeyboard(path)) == serialized);
        for (const auto& entry : std::filesystem::directory_iterator(directory)) Require(entry.path() == path);
        std::filesystem::remove_all(directory);
        std::cout << "input_configuration: passed\n";
    } catch (...) {
        std::filesystem::remove_all(directory);
        throw;
    }
}

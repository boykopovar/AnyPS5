#ifndef ANYPS5_INPUT_CONFIGURATION_HPP
#define ANYPS5_INPUT_CONFIGURATION_HPP

#include "prx/libScePad/include/InputMapping.hpp"
#include "SDL_gamecontroller.h"
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace InputConfig {

struct Action {
    std::string_view name;
    Pad::InputControl control;
    Pad::PadButton button;
};

std::filesystem::path ResolvePath();
std::span<const Action> Actions();
std::vector<Pad::InputBinding> LoadKeyboard(const std::filesystem::path& path, bool required = false);
std::string BindingName(const Pad::InputBinding& binding);
Pad::InputBinding ParseBinding(const Action& action, std::string_view source);
bool Matches(const Pad::InputBinding& binding, const Action& action);
std::string SerializeKeyboard(const std::vector<Pad::InputBinding>& bindings);
void WriteAtomic(const std::filesystem::path& path, std::string_view text);

}

#endif

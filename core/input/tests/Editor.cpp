#define SDL_MAIN_HANDLED
#include "InputEditor.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void Frame(InputConfig::Editor& editor) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) editor.HandleEvent(event);
    editor.Render();
}
void Click(InputConfig::Editor& editor, int x, int y) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION; event.motion.windowID = editor.WindowId(); event.motion.x = x; event.motion.y = y;
    editor.HandleEvent(event); editor.Render();
    event = {}; event.type = SDL_MOUSEBUTTONDOWN; event.button.windowID = editor.WindowId(); event.button.button = SDL_BUTTON_LEFT; event.button.x = x; event.button.y = y;
    editor.HandleEvent(event); editor.Render();
    event.type = SDL_MOUSEBUTTONUP;
    editor.HandleEvent(event); editor.Render();
}
int main() {
    SDL_SetMainReady();
    Require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, SDL_GetError());
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-editor-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
        const int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 1);
        Require(device >= 0, SDL_GetError());
        {
            InputConfig::Editor editor(directory / "anyps5-input.ini");
            editor.Open();
            const auto id = editor.WindowId();
            auto* window = SDL_GetWindowFromID(id);
            Require(window != nullptr && editor.IsOpen(), "editor window did not open");
            Frame(editor); Frame(editor);
            auto* renderer = SDL_GetRenderer(window);
            int width = 0, height = 0;
            Require(renderer != nullptr && SDL_GetRendererOutputSize(renderer, &width, &height) == 0, "missing editor renderer");
            const auto screenshot = [&](const std::string& suffix) {
                const auto* destination = std::getenv("ANYPS5_EDITOR_SCREENSHOT");
                if (destination == nullptr) return;
                std::filesystem::path screenshotPath(destination);
                if (!suffix.empty()) screenshotPath = screenshotPath.parent_path() / (screenshotPath.stem().string() + suffix + screenshotPath.extension().string());
                auto* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
                Require(surface != nullptr, SDL_GetError());
                Require(SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0, SDL_GetError());
                Require(SDL_SaveBMP(surface, screenshotPath.string().c_str()) == 0, SDL_GetError());
                SDL_FreeSurface(surface);
            };
            screenshot("");
            Require(std::filesystem::is_empty(directory), "opening editor wrote configuration without saving");
            Click(editor, 90, 224);
            Click(editor, 260, 46);
            Click(editor, 90, 224);
            SDL_Event key{}; key.type = SDL_KEYDOWN; key.key.windowID = id; key.key.keysym.scancode = SDL_SCANCODE_Z; key.key.keysym.sym = SDLK_z;
            editor.HandleEvent(key); editor.Render();
            key.type = SDL_KEYUP; editor.HandleEvent(key); editor.Render();
            Click(editor, 70, 78);
            Require(std::filesystem::exists(directory / "anyps5-input.ini"), "Save did not create the input configuration");
            bool captured = false;
            int crossBindings = 0;
            for (const auto& binding : InputConfig::LoadKeyboard(directory / "anyps5-input.ini")) if (binding.button == Pad::PadButton::Cross) { ++crossBindings; if (binding.key == SDL_SCANCODE_Z) captured = true; }
            Require(captured && crossBindings == 3, "capture cancellation or alternate key save failed");
            Click(editor, 240, 70); Frame(editor); screenshot("-controller");
            Click(editor, 680, 293);
            SDL_Event button{}; button.type = SDL_CONTROLLERBUTTONDOWN; button.cbutton.which = SDL_JoystickGetDeviceInstanceID(device); button.cbutton.button = SDL_CONTROLLER_BUTTON_Y;
            editor.HandleEvent(button); editor.Render();
            Click(editor, 70, 102);
            const auto profiles = InputConfig::LoadControllers(directory / "anyps5-controller.ini");
            Require(profiles.size() == 1 && profiles.begin()->second.buttons[0] == SDL_CONTROLLER_BUTTON_Y, "captured PS5 button source was not saved");
            Click(editor, 360, 70); Frame(editor); screenshot("-sdl");
            Click(editor, 100, 255);
            button = {}; button.type = SDL_JOYBUTTONDOWN; button.jbutton.which = SDL_JoystickGetDeviceInstanceID(device); button.jbutton.button = 2;
            editor.HandleEvent(button); editor.Render();
            Require(!std::filesystem::exists(directory / "anyps5-gamecontrollerdb.txt"), "physical mapping saved without confirmation");
            Click(editor, 50, 311);
            Click(editor, 70, 102);
            const auto mappings = InputConfig::LoadDatabase(directory / "anyps5-gamecontrollerdb.txt");
            Require(mappings.size() == 1 && mappings.front().find("a:b2,") != std::string::npos, "confirmed physical mapping was not saved");
            const int triggerStep = static_cast<int>(SDL_CONTROLLER_BUTTON_MAX) + static_cast<int>(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            for (int step = 1; step < triggerStep; ++step) Click(editor, 260, 255);
            Click(editor, 100, 255);
            button = {}; button.type = SDL_JOYBUTTONDOWN; button.jbutton.which = SDL_JoystickGetDeviceInstanceID(device); button.jbutton.button = 3;
            editor.HandleEvent(button); editor.Render();
            Click(editor, 50, 311); Click(editor, 70, 102);
            const auto digitalMappings = InputConfig::LoadDatabase(directory / "anyps5-gamecontrollerdb.txt");
            Require(digitalMappings.size() == 1 && digitalMappings.front().find("lefttrigger:b3,") != std::string::npos, "digital trigger capture was not saved");
            SDL_SetWindowSize(window, 640, 480); Frame(editor);
            SDL_Event close{};
            close.type = SDL_WINDOWEVENT; close.window.windowID = id; close.window.event = SDL_WINDOWEVENT_CLOSE;
            editor.HandleEvent(close); editor.Render();
            Require(!editor.IsOpen(), "editor window close did not close editor");
            editor.Open(); Frame(editor); editor.Close();
        }
        SDL_JoystickDetachVirtual(device);
        SDL_Quit(); std::filesystem::remove_all(directory);
        std::cout << "input_editor: passed\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; SDL_Quit(); std::filesystem::remove_all(directory); return 1;
    }
}

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
int main() {
    SDL_SetMainReady();
    Require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, SDL_GetError());
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-editor-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    try {
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
            if (const auto* screenshot = std::getenv("ANYPS5_EDITOR_SCREENSHOT")) {
                auto* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
                Require(surface != nullptr, SDL_GetError());
                Require(SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0, SDL_GetError());
                Require(SDL_SaveBMP(surface, screenshot) == 0, SDL_GetError());
                SDL_FreeSurface(surface);
            }
            SDL_SetWindowSize(window, 640, 480); Frame(editor);
            SDL_Event close{};
            close.type = SDL_WINDOWEVENT; close.window.windowID = id; close.window.event = SDL_WINDOWEVENT_CLOSE;
            editor.HandleEvent(close); editor.Render();
            Require(!editor.IsOpen(), "editor window close did not close editor");
            editor.Open(); Frame(editor); editor.Close();
            Require(std::filesystem::is_empty(directory), "opening editor wrote configuration without saving");
        }
        SDL_Quit(); std::filesystem::remove_all(directory);
        std::cout << "input_editor: passed\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; SDL_Quit(); std::filesystem::remove_all(directory); return 1;
    }
}

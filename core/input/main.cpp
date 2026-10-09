#define SDL_MAIN_HANDLED
#include "InputEditor.hpp"
#include <cstdio>
#include <stdexcept>

int main(int argc, char** argv) {
    SDL_SetMainReady();
    try {
        auto path = InputConfig::ResolvePath();
        if (argc == 3 && std::string_view(argv[1]) == "--config") path = std::filesystem::absolute(argv[2]);
        else if (argc != 1) throw std::runtime_error("Usage: anyps5-input-config [--config path/to/anyps5-input.ini]");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) throw std::runtime_error(SDL_GetError());
        {
            InputConfig::ApplyDatabase(InputConfig::LoadDatabase(path.parent_path() / "anyps5-gamecontrollerdb.txt"));
            InputConfig::Editor editor(path);
            editor.Open();
            while (editor.IsOpen()) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) editor.Close();
                    else editor.HandleEvent(event);
                }
                const auto saved = editor.Render();
                if (saved == InputConfig::SavedConfiguration::Database) {
                    InputConfig::ApplyDatabase(InputConfig::LoadDatabase(path.parent_path() / "anyps5-gamecontrollerdb.txt"));
                    editor.RefreshController();
                }
                SDL_Delay(10);
            }
        }
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AnyPS5 Input Configuration", error.what(), nullptr);
        SDL_Quit();
        return 1;
    }
}

#ifndef ANYPS5_INPUT_EDITOR_HPP
#define ANYPS5_INPUT_EDITOR_HPP

#include "ControllerMapping.hpp"
#include "SDL.h"
#include <memory>

namespace InputConfig {

enum class SavedConfiguration { None, Keyboard, Controller, Database };

class Editor {
public:
    explicit Editor(std::filesystem::path configPath);
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    void Open();
    void Close();
    void RefreshController();
    bool IsOpen() const;
    unsigned WindowId() const;
    bool HandleEvent(const SDL_Event& event);
    SavedConfiguration Render();

private:
    struct State;
    std::unique_ptr<State> state;
};

}
#endif

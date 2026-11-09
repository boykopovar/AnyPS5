#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTMENU_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTMENU_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace HostMenu {

enum class Action : std::uint8_t {
    VolumeUp,
    VolumeDown,
    ToggleMute,
    Screenshot,
    TogglePerformanceOverlay,
    ShowPadStatus,
};

struct Entry {
    std::optional<Action> action;
    std::string_view label;
};

class Menu {
public:
    Menu()
        : entries{{Action::VolumeUp, "VOLUME UP"},
                  {Action::VolumeDown, "VOLUME DOWN"},
                  {Action::ToggleMute, "MUTE"},
                  {Action::Screenshot, "SCREENSHOT"},
                  {Action::TogglePerformanceOverlay, "PERFORMANCE OVERLAY"},
                  {Action::ShowPadStatus, "PAD STATUS"},
                  {std::nullopt, "CLOSE"}} {}

    bool IsOpen() const { return open; }
    std::size_t Cursor() const { return cursor; }
    const std::vector<Entry>& Entries() const { return entries; }

    void Toggle() {
        open = !open;
        cursor = 0;
    }

    void Up() {
        if (!open) return;
        cursor = cursor == 0 ? entries.size() - 1 : cursor - 1;
    }

    void Down() {
        if (!open) return;
        cursor = (cursor + 1) % entries.size();
    }

    std::optional<Action> Activate() {
        if (!open) return std::nullopt;
        const auto action = entries[cursor].action;
        open = false;
        return action;
    }

private:
    std::vector<Entry> entries;
    std::size_t cursor = 0;
    bool open = false;
};

}  // namespace HostMenu

#endif

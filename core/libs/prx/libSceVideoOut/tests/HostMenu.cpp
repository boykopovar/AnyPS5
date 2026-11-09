#include "prx/libSceVideoOut/include/HostMenu.hpp"

#include <cstdio>
#include <optional>

namespace {

bool correct = true;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HOSTMENU: %s\n", message);
        correct = false;
    }
}

}  // namespace

int main() {
    HostMenu::Menu menu;
    Check(!menu.IsOpen(), "a new menu is open");
    Check(!menu.Activate().has_value(), "a closed menu activates an action");

    menu.Toggle();
    Check(menu.IsOpen() && menu.Cursor() == 0, "toggling does not open the menu at the first entry");
    menu.Up();
    Check(menu.Cursor() == menu.Entries().size() - 1, "moving up from the first entry does not wrap to the last");
    menu.Down();
    Check(menu.Cursor() == 0, "moving down from the last entry does not wrap to the first");
    menu.Down();
    Check(menu.Cursor() == 1, "moving down by one does not advance the cursor");

    const auto action = menu.Activate();
    Check(action.has_value() && *action == HostMenu::Action::VolumeDown, "activating the second entry is not volume down");
    Check(!menu.IsOpen(), "activating an action does not close the menu");

    menu.Toggle();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Up();
    menu.Down();
    const auto closing = menu.Activate();
    Check(!closing.has_value() && !menu.IsOpen(), "the Close entry does not close the menu without an action");

    menu.Toggle();
    menu.Toggle();
    Check(!menu.IsOpen(), "toggling an open menu does not close it");
    return correct ? 0 : 1;
}

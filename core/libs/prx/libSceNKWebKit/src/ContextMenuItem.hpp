#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace NKWebKit {

enum class ContextMenuItemType : std::uint32_t {
    Action = 0,
    CheckableAction = 1,
    Separator = 2,
    Submenu = 3
};

class ContextMenuItem final : public ObjectOf<ObjectType::ContextMenuItem> {
public:
    ContextMenuItem() = default;

    ContextMenuItemType type = ContextMenuItemType::Action;
    std::uint32_t tag = 0;
    Retained<String> title;
    bool enabled = true;
    bool checked = false;
    Retained<Object> userData;
    std::vector<Retained<ContextMenuItem>> submenuItems;
};

}

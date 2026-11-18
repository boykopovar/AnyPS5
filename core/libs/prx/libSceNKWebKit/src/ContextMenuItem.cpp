#include "prx/libSceNKWebKit/src/ContextMenuItem.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKContextMenuItemCreateAsAction(std::uint32_t tag, const void* titleRef, bool enabled) {
    auto item = NKWebKit::Make<NKWebKit::ContextMenuItem>();
    item->type = NKWebKit::ContextMenuItemType::Action;
    item->tag = tag;
    if (titleRef) {
        item->title = NKWebKit::Retained<NKWebKit::String>::Share(NKWebKit::Cast<NKWebKit::String>(titleRef, __func__));
    }
    item->enabled = enabled;
    return NKWebKit::ToRef(std::move(item));
}

const void* APS5_VABI WKContextMenuItemCreateAsCheckableAction(std::uint32_t tag, const void* titleRef, bool enabled, bool checked) {
    auto item = NKWebKit::Make<NKWebKit::ContextMenuItem>();
    item->type = NKWebKit::ContextMenuItemType::CheckableAction;
    item->tag = tag;
    if (titleRef) {
        item->title = NKWebKit::Retained<NKWebKit::String>::Share(NKWebKit::Cast<NKWebKit::String>(titleRef, __func__));
    }
    item->enabled = enabled;
    item->checked = checked;
    return NKWebKit::ToRef(std::move(item));
}

const void* APS5_VABI WKContextMenuItemCreateAsSubmenu(const void* titleRef, bool enabled, const void* submenuItemsRef) {
    auto item = NKWebKit::Make<NKWebKit::ContextMenuItem>();
    item->type = NKWebKit::ContextMenuItemType::Submenu;
    if (titleRef) {
        item->title = NKWebKit::Retained<NKWebKit::String>::Share(NKWebKit::Cast<NKWebKit::String>(titleRef, __func__));
    }
    item->enabled = enabled;
    if (submenuItemsRef) {
        const auto* array = NKWebKit::Cast<NKWebKit::Array>(submenuItemsRef, __func__);
        item->submenuItems.reserve(array->elements.size());
        for (const auto& elem : array->elements) {
            item->submenuItems.push_back(NKWebKit::Retained<NKWebKit::ContextMenuItem>::Share(NKWebKit::Cast<NKWebKit::ContextMenuItem>(elem.Get(), __func__)));
        }
    }
    return NKWebKit::ToRef(std::move(item));
}

const void* APS5_VABI WKContextMenuItemSeparatorItem() {
    auto item = NKWebKit::Make<NKWebKit::ContextMenuItem>();
    item->type = NKWebKit::ContextMenuItemType::Separator;
    item->enabled = true;
    return NKWebKit::ToRef(std::move(item));
}

std::uint32_t APS5_VABI WKContextMenuItemGetType(const void* itemRef) {
    return static_cast<std::uint32_t>(NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__)->type);
}

std::uint32_t APS5_VABI WKContextMenuItemGetTag(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__)->tag;
}

const void* APS5_VABI WKContextMenuItemCopyTitle(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__);
    if (!item->title || item->title->null) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    }
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(item->title.Get()));
}

bool APS5_VABI WKContextMenuItemGetEnabled(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__)->enabled;
}

bool APS5_VABI WKContextMenuItemGetChecked(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__)->checked;
}

const void* APS5_VABI WKContextMenuItemGetUserData(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__);
    return item->userData ? item->userData.Get() : nullptr;
}

void APS5_VABI WKContextMenuItemSetUserData(const void* itemRef, const void* userDataRef) {
    auto* item = NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__);
    item->userData = userDataRef ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(userDataRef, __func__))) : nullptr;
}

const void* APS5_VABI WKContextMenuCopySubmenuItems(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::ContextMenuItem>(itemRef, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(item->submenuItems.size());
    for (const auto& sub : item->submenuItems) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(sub.Get()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

}

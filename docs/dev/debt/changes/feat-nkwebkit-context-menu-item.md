# libSceNKWebKit

## add Functional

- [Context menu item and submenu helpers](../../../core/libs/prx/libSceNKWebKit/src/ContextMenuItem.cpp) (libSceNKWebKit) - `WKContextMenuItemCreateAsAction`, `WKContextMenuItemCreateAsCheckableAction`, `WKContextMenuItemCreateAsSubmenu`, `WKContextMenuItemSeparatorItem`, `WKContextMenuItemGetType`, `WKContextMenuItemGetTag`, `WKContextMenuItemCopyTitle`, `WKContextMenuItemGetEnabled`, `WKContextMenuItemGetChecked`, `WKContextMenuItemGetUserData`, `WKContextMenuItemSetUserData` and `WKContextMenuCopySubmenuItems` follow [WKContextMenuItem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/WKContextMenuItem.cpp). Accessors throw `std::invalid_argument` on null or wrong object type. Console ABI is unverified.

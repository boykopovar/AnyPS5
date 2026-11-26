# libSceNKWebKit

## add Functional

- [DisplayList::DrawNativeImage](../../../core/libs/prx/libSceNKWebKit/src/DisplayListNativeImage.cpp) (libSceNKWebKit) - the item is the 96-byte Cairo layout of the 2020 source (`DrawingItem`, image size, destination and source rectangles, `ImagePaintingOptions`), not measured on a console. The Cairo build does not store the native image, so the constructor validates and ignores it, `apply` is a no-op and `localBounds` returns the destination rectangle, matching WebKit's Cairo `DrawNativeImage`. Closes when: the layout is checked against a PS5.

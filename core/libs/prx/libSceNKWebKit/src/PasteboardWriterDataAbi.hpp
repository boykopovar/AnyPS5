#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct PasteboardPlainText {
    bool canSmartCopyOrDelete;
    WtfAbi::String text;
};

struct PasteboardURLData {
    WtfAbi::URL url;
    WtfAbi::String title;
};

template <typename T>
struct PasteboardOptional {
    alignas(T) unsigned char storage[sizeof(T)];
    bool engaged;

    T& value() { return *reinterpret_cast<T*>(storage); }
};

struct PasteboardWebContent {
    unsigned char empty;
};

struct PasteboardWriterData {
    PasteboardOptional<PasteboardPlainText> plainText;
    PasteboardOptional<PasteboardURLData> url;
    PasteboardOptional<PasteboardWebContent> webContent;
};

static_assert(sizeof(PasteboardPlainText) == 16 && offsetof(PasteboardPlainText, text) == 8);
static_assert(sizeof(PasteboardURLData) == 48 && offsetof(PasteboardURLData, title) == 0x28);
static_assert(sizeof(PasteboardWriterData) == 88 && alignof(PasteboardWriterData) == 8);
static_assert(offsetof(PasteboardWriterData, plainText) == 0x00 && offsetof(PasteboardWriterData, plainText.engaged) == 0x10);
static_assert(offsetof(PasteboardWriterData, url) == 0x18 && offsetof(PasteboardWriterData, url.engaged) == 0x48);
static_assert(offsetof(PasteboardWriterData, webContent) == 0x50 && offsetof(PasteboardWriterData, webContent.engaged) == 0x51);
static_assert(std::is_standard_layout_v<PasteboardWriterData>);

}  // namespace NKWebKit::WebCoreAbi

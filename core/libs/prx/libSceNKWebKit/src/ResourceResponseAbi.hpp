#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct StringSlot {
    WtfAbi::StringImpl* impl;
};

struct ResponseHeaderStorage {
    alignas(8) unsigned char storage[32];
};

struct ResourceResponseBase {
    WtfAbi::URL url;
    StringSlot mimeType;
    long long expectedContentLength;
    StringSlot textEncodingName;
    StringSlot httpStatusText;
    StringSlot httpVersion;
    ResponseHeaderStorage httpHeaderFields;
};

static_assert(sizeof(StringSlot) == 8);
static_assert(sizeof(ResponseHeaderStorage) == 32);
static_assert(offsetof(ResourceResponseBase, url) == 0);
static_assert(offsetof(ResourceResponseBase, mimeType) == 40);
static_assert(offsetof(ResourceResponseBase, expectedContentLength) == 48);
static_assert(offsetof(ResourceResponseBase, textEncodingName) == 56);
static_assert(offsetof(ResourceResponseBase, httpStatusText) == 64);
static_assert(offsetof(ResourceResponseBase, httpVersion) == 72);
static_assert(offsetof(ResourceResponseBase, httpHeaderFields) == 80);

}

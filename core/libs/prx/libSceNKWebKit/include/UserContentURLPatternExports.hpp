#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct UserContentURLPattern {
    bool invalid;
    NKWebKit::WtfAbi::String scheme;
    NKWebKit::WtfAbi::String host;
    NKWebKit::WtfAbi::String path;
    bool matchSubdomains;
};

static_assert(sizeof(UserContentURLPattern) == 40);
static_assert(offsetof(UserContentURLPattern, scheme) == 8);
static_assert(offsetof(UserContentURLPattern, host) == 16);
static_assert(offsetof(UserContentURLPattern, path) == 24);
static_assert(offsetof(UserContentURLPattern, matchSubdomains) == 32);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZN7WebCore21UserContentURLPattern5parseERKN3WTF6StringE(NKWebKit::WebCoreAbi::UserContentURLPattern* self, const NKWebKit::WtfAbi::String* pattern);
bool APS5_VABI _ZNK7WebCore21UserContentURLPattern7matchesERKN3WTF3URLE(const NKWebKit::WebCoreAbi::UserContentURLPattern* self, const NKWebKit::WtfAbi::URL* url);

}

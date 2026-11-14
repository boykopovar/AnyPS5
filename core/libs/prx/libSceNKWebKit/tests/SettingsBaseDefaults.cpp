#include "prx/libSceNKWebKit/include/SettingsBaseDefaultsExports.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using NKWebKit::WtfAbi::String;

static_assert(sizeof(String) == 8);
static_assert(sizeof(NKWebKit::WtfAbi::StringImpl) == 24);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SettingsBase defaults check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}  // namespace

int main() {
    Require(_ZN7WebCore12SettingsBase26defaultMinimumZoomFontSizeEv() == 15.0f);
    Require(!_ZN7WebCore12SettingsBase28defaultTextAutosizingEnabledEv());
    Require(_ZN7WebCore12SettingsBase37defaultDownloadableBinaryFontsEnabledEv());
    Require(!_ZN7WebCore12SettingsBase35defaultContentChangeObserverEnabledEv());

    const String& first = _ZN7WebCore12SettingsBase48defaultMediaContentTypesRequiringHardwareSupportEv();
    const String& second = _ZN7WebCore12SettingsBase48defaultMediaContentTypesRequiringHardwareSupportEv();
    Require(&first == &second);
    Require(first.impl != nullptr);
    Require(first.impl->length == 0);
    Require(first.impl == second.impl);
    return 0;
}

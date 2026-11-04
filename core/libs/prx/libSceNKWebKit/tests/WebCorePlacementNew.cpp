#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

using PlacementNew = void* (APS5_VABI*)(std::size_t, void*);
using NotNullPlacementNew = void* (APS5_VABI*)(std::size_t, unsigned int, void*);

struct Entry {
    const char* name;
    PlacementNew function;
};

struct NotNullEntry {
    const char* name;
    NotNullPlacementNew function;
};

static const Entry entries[] = {
    {"_ZN7WebCore11MediaPlayernaEmPv", &_ZN7WebCore11MediaPlayernaEmPv},
    {"_ZN7WebCore11MediaPlayernwEmPv", &_ZN7WebCore11MediaPlayernwEmPv},
    {"_ZN7WebCore13MIMETypeCachenaEmPv", &_ZN7WebCore13MIMETypeCachenaEmPv},
    {"_ZN7WebCore13MIMETypeCachenwEmPv", &_ZN7WebCore13MIMETypeCachenwEmPv},
    {"_ZN7WebCore14StaticNodeListnaEmPv", &_ZN7WebCore14StaticNodeListnaEmPv},
    {"_ZN7WebCore14StaticNodeListnwEmPv", &_ZN7WebCore14StaticNodeListnwEmPv},
    {"_ZN7WebCore15AsyncFileStreamnaEmPv", &_ZN7WebCore15AsyncFileStreamnaEmPv},
    {"_ZN7WebCore15AsyncFileStreamnwEmPv", &_ZN7WebCore15AsyncFileStreamnwEmPv},
    {"_ZN7WebCore16BlobRegistryImplnaEmPv", &_ZN7WebCore16BlobRegistryImplnaEmPv},
    {"_ZN7WebCore16BlobRegistryImplnwEmPv", &_ZN7WebCore16BlobRegistryImplnwEmPv},
    {"_ZN7WebCore16TrackPrivateBasenaEmPv", &_ZN7WebCore16TrackPrivateBasenaEmPv},
    {"_ZN7WebCore16TrackPrivateBasenwEmPv", &_ZN7WebCore16TrackPrivateBasenwEmPv},
    {"_ZN7WebCore17NowPlayingManagernaEmPv", &_ZN7WebCore17NowPlayingManagernaEmPv},
    {"_ZN7WebCore17NowPlayingManagernwEmPv", &_ZN7WebCore17NowPlayingManagernwEmPv},
    {"_ZN7WebCore17PageConsoleClientnaEmPv", &_ZN7WebCore17PageConsoleClientnaEmPv},
    {"_ZN7WebCore17PageConsoleClientnwEmPv", &_ZN7WebCore17PageConsoleClientnwEmPv},
    {"_ZN7WebCore18PrewarmInformationnwEmPv", &_ZN7WebCore18PrewarmInformationnwEmPv},
    {"_ZN7WebCore18TextureMapperLayernwEmPv", &_ZN7WebCore18TextureMapperLayernwEmPv},
    {"_ZN7WebCore21MediaRecorderProvidernaEmPv", &_ZN7WebCore21MediaRecorderProvidernaEmPv},
    {"_ZN7WebCore21MediaRecorderProvidernwEmPv", &_ZN7WebCore21MediaRecorderProvidernwEmPv},
    {"_ZN7WebCore28InspectorFrontendClientLocal8SettingsnaEmPv", &_ZN7WebCore28InspectorFrontendClientLocal8SettingsnaEmPv},
    {"_ZN7WebCore28InspectorFrontendClientLocal8SettingsnwEmPv", &_ZN7WebCore28InspectorFrontendClientLocal8SettingsnwEmPv},
};

static const NotNullEntry notNullEntries[] = {
    {"_ZN7WebCore11MediaPlayernwEm10NotNullTagPv", &_ZN7WebCore11MediaPlayernwEm10NotNullTagPv},
    {"_ZN7WebCore16BlobRegistryImplnwEm10NotNullTagPv", &_ZN7WebCore16BlobRegistryImplnwEm10NotNullTagPv},
    {"_ZN7WebCore16TrackPrivateBasenwEm10NotNullTagPv", &_ZN7WebCore16TrackPrivateBasenwEm10NotNullTagPv},
    {"_ZN7WebCore17PageConsoleClientnwEm10NotNullTagPv", &_ZN7WebCore17PageConsoleClientnwEm10NotNullTagPv},
    {"_ZN7WebCore18TextureMapperLayernwEm10NotNullTagPv", &_ZN7WebCore18TextureMapperLayernwEm10NotNullTagPv},
};

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "placement new check failed for %s at line %d\n", name, line);
        std::abort();
    }
}

int main() {
    alignas(16) unsigned char buffer[256];
    const std::size_t sizes[] = {0, 1, 8, 64, sizeof(buffer), ~static_cast<std::size_t>(0)};
    for (const Entry& entry : entries) {
        for (std::size_t size : sizes) {
            Check(entry.function(size, buffer) == buffer, entry.name, __LINE__);
            Check(entry.function(size, buffer + 1) == buffer + 1, entry.name, __LINE__);
        }
        Check(entry.function(8, nullptr) == nullptr, entry.name, __LINE__);
    }
    for (const NotNullEntry& entry : notNullEntries) {
        for (std::size_t size : sizes) {
            Check(entry.function(size, 0, buffer) == buffer, entry.name, __LINE__);
            Check(entry.function(size, 0, buffer + 1) == buffer + 1, entry.name, __LINE__);
        }
    }
    Check(sizeof(entries) / sizeof(entries[0]) == 22, "entries", __LINE__);
    Check(sizeof(notNullEntries) / sizeof(notNullEntries[0]) == 5, "notNullEntries", __LINE__);
    return 0;
}

#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <utility>

extern "C" {

void APS5_VABI WKContextSetAdditionalPluginsDirectory(const void* contextRef, const void* pluginsDirectoryRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pluginsDirectoryRef) {
        pool->additionalPluginsDirectory = nullptr;
        return;
    }
    pool->additionalPluginsDirectory = NKWebKit::Retained<NKWebKit::String>::Share(
        NKWebKit::Cast<NKWebKit::String>(pluginsDirectoryRef, __func__));
}

void APS5_VABI WKContextRefreshPlugIns(const void* contextRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    ++pool->refreshPlugInsCount;
}

void APS5_VABI WKContextClearSupportedPlugins(const void* contextRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->supportedPlugins.clear();
}

void APS5_VABI WKContextAddSupportedPlugin(
    const void* contextRef,
    const void* domainRef,
    const void* nameRef,
    const void* mimeTypesRef,
    const void* extensionsRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    NKWebKit::SupportedPlugin plugin;
    if (domainRef) {
        plugin.domain = NKWebKit::Retained<NKWebKit::String>::Share(
            NKWebKit::Cast<NKWebKit::String>(domainRef, __func__));
    }
    if (nameRef) {
        plugin.name = NKWebKit::Retained<NKWebKit::String>::Share(
            NKWebKit::Cast<NKWebKit::String>(nameRef, __func__));
    }
    if (mimeTypesRef) {
        plugin.mimeTypes = NKWebKit::Retained<NKWebKit::Array>::Share(
            NKWebKit::Cast<NKWebKit::Array>(mimeTypesRef, __func__));
    }
    if (extensionsRef) {
        plugin.extensions = NKWebKit::Retained<NKWebKit::Array>::Share(
            NKWebKit::Cast<NKWebKit::Array>(extensionsRef, __func__));
    }
    pool->supportedPlugins.push_back(std::move(plugin));
}

void APS5_VABI WKContextSetPlugInAutoStartOriginHashes(const void* contextRef, const void* dictionaryRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!dictionaryRef) {
        pool->plugInAutoStartOriginHashes = nullptr;
        return;
    }
    pool->plugInAutoStartOriginHashes = NKWebKit::Retained<NKWebKit::Dictionary>::Share(
        NKWebKit::Cast<NKWebKit::Dictionary>(dictionaryRef, __func__));
}

void APS5_VABI WKContextSetPlugInAutoStartOrigins(const void* contextRef, const void* arrayRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!arrayRef) {
        pool->plugInAutoStartOrigins = nullptr;
        return;
    }
    pool->plugInAutoStartOrigins = NKWebKit::Retained<NKWebKit::Array>::Share(
        NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__));
}

void APS5_VABI WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(
    const void* contextRef,
    const void* arrayRef,
    double time) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!arrayRef) {
        pool->plugInAutoStartOrigins = nullptr;
    } else {
        pool->plugInAutoStartOrigins = NKWebKit::Retained<NKWebKit::Array>::Share(
            NKWebKit::Cast<NKWebKit::Array>(arrayRef, __func__));
    }
    pool->plugInAutoStartOriginsFilterTime = time;
}

const void* APS5_VABI WKContextCopyPlugInAutoStartOriginHashes(const void* contextRef) {
    auto pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->plugInAutoStartOriginHashes) {
        return NKWebKit::Make<NKWebKit::Dictionary>().Leak();
    }
    return NKWebKit::Retained<NKWebKit::Dictionary>::Share(pool->plugInAutoStartOriginHashes.Get()).Leak();
}

}

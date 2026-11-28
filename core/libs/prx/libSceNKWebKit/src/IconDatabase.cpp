#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKContextGetIconDatabase(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->iconDatabase) {
        pool->iconDatabase = NKWebKit::Make<NKWebKit::IconDatabase>();
    }
    return pool->iconDatabase.Get();
}

void APS5_VABI WKContextSetIconDatabasePath(const void* contextRef, const void* iconDatabasePathRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!iconDatabasePathRef) {
        pool->iconDatabasePath.reset();
        return;
    }
    pool->iconDatabasePath = NKWebKit::OptionalString(iconDatabasePathRef, __func__);
}

void APS5_VABI WKIconDatabaseRetainIconForURL(const void* iconDatabaseRef, const void* pageURLRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* pageURL = NKWebKit::Cast<NKWebKit::URL>(pageURLRef, __func__);
    iconDatabase->iconRetainCounts[pageURL->text]++;
}

void APS5_VABI WKIconDatabaseReleaseIconForURL(const void* iconDatabaseRef, const void* pageURLRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* pageURL = NKWebKit::Cast<NKWebKit::URL>(pageURLRef, __func__);
    auto it = iconDatabase->iconRetainCounts.find(pageURL->text);
    if (it != iconDatabase->iconRetainCounts.end()) {
        if (it->second > 1) {
            --it->second;
        } else {
            iconDatabase->iconRetainCounts.erase(it);
        }
    }
}

const void* APS5_VABI WKIconDatabaseCopyIconURLForPageURL(const void* iconDatabaseRef, const void* pageURLRef) {
    const auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* pageURL = NKWebKit::Cast<NKWebKit::URL>(pageURLRef, __func__);
    auto it = iconDatabase->pageURLToIconURL.find(pageURL->text);
    if (it == iconDatabase->pageURLToIconURL.end()) {
        return nullptr;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(it->second));
}

void APS5_VABI WKIconDatabaseSetIconURLForPageURL(const void* iconDatabaseRef, const void* iconURLRef, const void* pageURLRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* iconURL = NKWebKit::Cast<NKWebKit::URL>(iconURLRef, __func__);
    const auto* pageURL = NKWebKit::Cast<NKWebKit::URL>(pageURLRef, __func__);
    iconDatabase->pageURLToIconURL[pageURL->text] = iconURL->text;
}

void APS5_VABI WKIconDatabaseSetIconDataForIconURL(const void* iconDatabaseRef, const void* iconDataRef, const void* iconURLRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* iconData = NKWebKit::Cast<NKWebKit::Data>(iconDataRef, __func__);
    const auto* iconURL = NKWebKit::Cast<NKWebKit::URL>(iconURLRef, __func__);
    iconDatabase->iconURLToData[iconURL->text] = iconData->bytes;
}

const void* APS5_VABI WKIconDatabaseCopyIconDataForPageURL(const void* iconDatabaseRef, const void* pageURLRef) {
    const auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    const auto* pageURL = NKWebKit::Cast<NKWebKit::URL>(pageURLRef, __func__);
    auto itUrl = iconDatabase->pageURLToIconURL.find(pageURL->text);
    if (itUrl == iconDatabase->pageURLToIconURL.end()) {
        return nullptr;
    }
    auto itData = iconDatabase->iconURLToData.find(itUrl->second);
    if (itData == iconDatabase->iconURLToData.end()) {
        return nullptr;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(itData->second));
}

void APS5_VABI WKIconDatabaseRemoveAllIcons(const void* iconDatabaseRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    iconDatabase->pageURLToIconURL.clear();
    iconDatabase->iconURLToData.clear();
    iconDatabase->iconRetainCounts.clear();
}

void APS5_VABI WKIconDatabaseCheckIntegrityBeforeOpening(const void* iconDatabaseRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    iconDatabase->checkIntegrityBeforeOpening = true;
}

void APS5_VABI WKIconDatabaseClose(const void* iconDatabaseRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    iconDatabase->closed = true;
}

void APS5_VABI WKIconDatabaseEnableDatabaseCleanup(const void* iconDatabaseRef) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    iconDatabase->cleanupEnabled = true;
}

void APS5_VABI WKIconDatabaseSetIconDatabaseClient(const void* iconDatabaseRef, const void* client) {
    auto* iconDatabase = NKWebKit::Cast<NKWebKit::IconDatabase>(iconDatabaseRef, __func__);
    iconDatabase->client = client;
}

}

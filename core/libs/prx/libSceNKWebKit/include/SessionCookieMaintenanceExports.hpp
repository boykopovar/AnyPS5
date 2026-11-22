#pragma once

#include "prx/libSceNKWebKit/src/CookieAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::CookieVector APS5_VABI _ZN7WebCore21NetworkStorageSession13getAllCookiesEv(const void* session);

void APS5_VABI _ZN7WebCore21NetworkStorageSession16deleteAllCookiesEv(const void* session);

void APS5_VABI _ZN7WebCore21NetworkStorageSession16flushCookieStoreEv(const void* session);

}

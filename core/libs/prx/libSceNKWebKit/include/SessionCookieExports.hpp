#pragma once

#include "prx/libSceNKWebKit/src/CookieAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::CookieVector APS5_VABI _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(const void* session, const NKWebKit::WtfAbi::URL* url);

void APS5_VABI _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(const void* session, const NKWebKit::WebCoreAbi::CookieVector* cookies, const NKWebKit::WtfAbi::URL* url, const NKWebKit::WtfAbi::URL* firstParty);

void APS5_VABI _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(const void* session, const NKWebKit::WebCoreAbi::Cookie* cookie);

}

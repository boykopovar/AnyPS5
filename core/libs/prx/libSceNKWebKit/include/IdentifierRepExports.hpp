#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/IdentifierRepAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::IdentifierRep* APS5_VABI _ZN7WebCore13IdentifierRep3getEPKc(const char* name);
NKWebKit::WebCoreAbi::IdentifierRep* APS5_VABI _ZN7WebCore13IdentifierRep3getEi(int intID);
bool APS5_VABI _ZN7WebCore13IdentifierRep7isValidEPS0_(NKWebKit::WebCoreAbi::IdentifierRep* identifier);

}

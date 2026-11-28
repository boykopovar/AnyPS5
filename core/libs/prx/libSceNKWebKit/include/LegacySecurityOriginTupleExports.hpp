#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/LegacySecurityOriginTupleAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::LegacyOptionalSecurityOriginTuple* APS5_VABI _ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(
    NKWebKit::WebCoreAbi::LegacyOptionalSecurityOriginTuple* result, const NKWebKit::WtfAbi::String* databaseIdentifier);

}

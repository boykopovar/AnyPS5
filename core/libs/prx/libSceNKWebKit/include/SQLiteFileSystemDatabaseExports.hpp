#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::OptionalWallTime APS5_VABI _ZN7WebCore16SQLiteFileSystem20databaseCreationTimeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& fileName);
bool APS5_VABI _ZN7WebCore16SQLiteFileSystem18deleteDatabaseFileERKN3WTF6StringE(const NKWebKit::WtfAbi::String& filePath);

}

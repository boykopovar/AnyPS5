#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/BlobDataFileReferenceAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16SQLiteFileSystem28appendDatabaseFileNameToPathERKN3WTF6StringES4_(const NKWebKit::WtfAbi::String& path, const NKWebKit::WtfAbi::String& fileName);
bool APS5_VABI _ZN7WebCore16SQLiteFileSystem28deleteEmptyDatabaseDirectoryERKN3WTF6StringE(const NKWebKit::WtfAbi::String& path);
NKWebKit::WebCoreAbi::OptionalWallTime APS5_VABI _ZN7WebCore16SQLiteFileSystem24databaseModificationTimeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& fileName);

}

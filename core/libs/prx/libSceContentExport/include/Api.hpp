#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* initParam);
int APS5_VABI sceContentExportFinish(std::int32_t expId);
int APS5_VABI sceContentExportFromData(std::int32_t expId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataCallback callback, void* userData, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportFromFile(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportAddToFavorites(std::int64_t contentId);
int APS5_VABI sceContentExportDataProvideFunction(void);
int APS5_VABI sceContentExportFree(void);
int APS5_VABI sceContentExportFrom(void);
int APS5_VABI sceContentExportFromFileForSf(std::int32_t expId, const ContentExportParam* param, const ContentExportSfParam* sfParam, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportFromFileWithContentIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const std::int64_t* contentIds, std::uint32_t contentCount, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportFromFileWithTitleIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportInit(const ContentExportInitParam* param);
int APS5_VABI sceContentExportMalloc(void);
int APS5_VABI sceContentExportRemoveFromFavorites(std::int64_t contentId);
int APS5_VABI sceContentExportRenameContent(std::int64_t contentId, const char* name);
int APS5_VABI sceContentExportValidateContents(const std::int64_t* contentIds, std::uint32_t count);
int APS5_VABI sceContentExportStart(void);
int APS5_VABI sceContentExportGetProgress(std::int32_t expId);
int APS5_VABI sceContentExportCancel(std::int32_t expId);
int APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t expId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufferLength);
int APS5_VABI sceContentExportTerm(void);

}

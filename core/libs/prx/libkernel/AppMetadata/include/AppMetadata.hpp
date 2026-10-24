#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_APPMETADATA_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_APPMETADATA_HPP

#include <cstdint>

struct AppTitle {
    char value[128];
};

struct AppTitleId {
    char value[12];
};

struct AppContentVersion {
    char value[16];
};

struct AppIconData {
    const std::uint8_t* bytes;
    std::uint64_t size;
};

extern "C" {

AppTitle GetAppTitle_nid_postfix();
AppTitleId GetAppTitleId_nid_postfix();
bool HasAppParamJson_nid_no_patch();
AppContentVersion GetAppContentVersion_nid_no_patch();
bool HasAppIcon_nid_postfix();
AppIconData GetAppIconData_nid_postfix();
std::uint64_t GetAppDownloadDataSizeMiB_nid_postfix();
std::uint32_t GetAppAttribute3_nid_no_patch();
std::int32_t GetAppUserDefinedParam_nid_postfix(std::uint32_t index);

}

#endif

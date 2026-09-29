#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_APPMETADATA_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_APPMETADATA_HPP

#include <cstdint>

struct AppTitle {
    char value[128];
};

struct AppTitleId {
    char value[12];
};

struct AppIconData {
    const std::uint8_t* bytes;
    std::uint64_t size;
};

extern "C" {

AppTitle GetAppTitle_nid_postfix();
AppTitleId GetAppTitleId_nid_postfix();
bool HasAppIcon_nid_postfix();
AppIconData GetAppIconData_nid_postfix();
// The download data quota param.json declares, in MiB (0 when none).
std::uint64_t GetAppDownloadDataSizeMiB_nid_postfix();

}

#endif

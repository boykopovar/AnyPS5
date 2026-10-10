#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceContentDeleteById();
int APS5_VABI sceContentExportGetProgress();
int APS5_VABI sceContentExportCancel();
int APS5_VABI sceContentExportFromDataWithThumbnail();
int APS5_VABI sceContentSearchGetContentLastUpdateId();
int APS5_VABI sceContentSearchOpenMetadataByContentId();
int APS5_VABI sceContentSearchGetTotalContentSize();
int APS5_VABI sceContentSearchGetNumOfContent();
}

namespace {

bool ReportsNotImplemented(const char* name, int (APS5_VABI *function)()) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == std::string(name) + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name, error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name);
    return false;
}

}

int main() {
    bool correct = true;
    correct &= ReportsNotImplemented("sceContentDeleteById", sceContentDeleteById);
    correct &= ReportsNotImplemented("sceContentExportGetProgress", sceContentExportGetProgress);
    correct &= ReportsNotImplemented("sceContentExportCancel", sceContentExportCancel);
    correct &= ReportsNotImplemented("sceContentExportFromDataWithThumbnail", sceContentExportFromDataWithThumbnail);
    correct &= ReportsNotImplemented("sceContentSearchGetContentLastUpdateId", sceContentSearchGetContentLastUpdateId);
    correct &= ReportsNotImplemented("sceContentSearchOpenMetadataByContentId", sceContentSearchOpenMetadataByContentId);
    correct &= ReportsNotImplemented("sceContentSearchGetTotalContentSize", sceContentSearchGetTotalContentSize);
    correct &= ReportsNotImplemented("sceContentSearchGetNumOfContent", sceContentSearchGetNumOfContent);
    return correct ? 0 : 1;
}

#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_ADDCONT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_ADDCONT_HPP

#include <filesystem>
#include <vector>

struct AddcontEntry {
    char label[17];
    bool hasData;
    std::filesystem::path directory;
};

extern "C" const std::vector<AddcontEntry>& AddcontEntries_nid_no_patch();

#endif

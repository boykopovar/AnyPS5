#ifndef RELINKER_ANALYSIS_PATHCONFIMPORT_HPP
#define RELINKER_ANALYSIS_PATHCONFIMPORT_HPP

#include <elfpatcher/general/ElfConstants.hpp>
#include <cstdint>
#include <string_view>

namespace Relinker {

inline bool IsPathconfImportLibrary(std::string_view library) {
    return library.empty() || library.starts_with("libkernel");
}

inline bool IsPathconfImportName(std::string_view nid) {
    const auto hashPos = nid.find('#');
    if (hashPos != std::string_view::npos) nid = nid.substr(0, hashPos);
    return nid == "pathconf" || nid == "oc98+zOImus" || nid == "pathconf_nid_postfix";
}

inline bool IsPathconfImportSymbolType(std::uint8_t symbolType) {
    return symbolType == Elfpatcher::STT_FUNC || symbolType == Elfpatcher::STT_NOTYPE;
}

inline unsigned PathconfRelocationRank(std::uint32_t relocationType) {
    if (relocationType == Elfpatcher::R_X86_64_JUMP_SLOT) return 3;
    if (relocationType == Elfpatcher::R_X86_64_GLOB_DAT) return 2;
    if (relocationType == Elfpatcher::R_X86_64_64) return 1;
    return 0;
}

}

#endif

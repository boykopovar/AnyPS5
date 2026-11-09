#include <nid/NidResolver.hpp>
#include <nid/ExportExclusions.hpp>
#include <nid/NidCompute.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace Nid {

bool IsSceImport(const std::string& name) {
    return name.ends_with(Internal::kNidPostfix) || (name.size() >= 3 &&
        std::tolower(static_cast<unsigned char>(name[0])) == 's' &&
        std::tolower(static_cast<unsigned char>(name[1])) == 'c' &&
        std::tolower(static_cast<unsigned char>(name[2])) == 'e');
}

std::unordered_map<std::string, std::string> ResolveImportNids(const std::vector<std::string>& names, const std::unordered_set<std::string>& excluded, const PatchOptions& options) {
    const std::unordered_set<std::string> unique(names.begin(), names.end());
    auto result = ResolveNids(std::vector<std::string>(unique.begin(), unique.end()), "", excluded);
    for (const auto& name : unique) {
        if ((!options.executable || options.onlySce) && !IsSceImport(name)) result.at(name) = name;
        else if (!options.executable) result.at(name) = ResolveOneName(name);
    }
    return result;
}

std::string ResolveOneName(const std::string& funcName) {
    return ResolveNids({funcName}, "", {}).at(funcName);
}

std::unordered_map<std::string, std::string> ResolveNids(const std::vector<std::string>& exportedNames, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports) {
    using namespace Internal;

    std::unordered_set<std::string> nameSet(exportedNames.begin(), exportedNames.end());

    if (nameSet.size() != exportedNames.size()) {
        for (std::size_t i = 0u; i < exportedNames.size(); ++i) {
            for (std::size_t j = i + 1u; j < exportedNames.size(); ++j) {
                if (exportedNames[i] == exportedNames[j])
                    throw std::runtime_error("duplicate exported symbol \"" + exportedNames[i] + "\" in library \"" + libraryName + "\"");
            }
        }
    }

    std::unordered_map<std::string, std::string> result;
    result.reserve(exportedNames.size());

    for (const std::string& name : exportedNames) {
        if (excludedExports.contains(NormalizeExportName(name))) {
            result[name] = name;
            continue;
        }
        if (IsNidNoPatchCut(name)) {
            result[name] = StripNidNoPatchCut(name);
            continue;
        }
        if (IsNidNoPatch(name)) {
            result[name] = name;
            continue;
        }

        const std::string stripped = StripNidPostfix(name);
        const bool hasPostfix = stripped != name;

        if (!hasPostfix && nameSet.count(name + kNidPostfix)) {
            result[name] = name;
            continue;
        }

        result[name] = ComputeNid(stripped, libraryName);
    }

    return result;
}

}

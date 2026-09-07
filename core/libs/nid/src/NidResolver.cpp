#include <nid/NidResolver.hpp>
#include <nid/NidCompute.hpp>
#include <cstdint>
#include <nid/NidPatcherUtils.hpp>
#include <stdexcept>
#include <unordered_set>

namespace Nid {

std::unordered_map<std::string, std::string> ResolveNids(const std::vector<std::string>& exportedNames, const std::string& libraryName) {
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

#ifndef NID_NIDRESOLVER_HPP
#define NID_NIDRESOLVER_HPP

#include <nid/IBinaryPatcher.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Nid {

bool IsSceImport(const std::string& name);

std::unordered_map<std::string, std::string> ResolveImportNids(const std::vector<std::string>& names, const std::unordered_set<std::string>& excluded, const PatchOptions& options);

std::string ResolveOneName(const std::string& funcName);

std::unordered_map<std::string, std::string> ResolveNids(const std::vector<std::string>& exportedNames, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports);

}

#endif

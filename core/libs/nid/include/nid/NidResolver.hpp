#ifndef NID_NIDRESOLVER_HPP
#define NID_NIDRESOLVER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Nid {

std::string ResolveOneName(const std::string& funcName);

std::unordered_map<std::string, std::string> ResolveNids(const std::vector<std::string>& exportedNames,
                                                         const std::string& libraryName,
                                                         const std::unordered_set<std::string>& excludedExports);

}

#endif

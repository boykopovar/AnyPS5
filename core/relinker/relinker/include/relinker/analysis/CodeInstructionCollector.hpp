#ifndef RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP
#define RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP

#include <domain/Types.hpp>
#include <set>
#include <functional>
#include <string_view>

namespace Relinker {

class CodeInstructionCollector {
public:
    std::set<Domain::VirtualAddress> Collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}, std::set<Domain::VirtualAddress>* entryRoots = nullptr, const std::function<void(std::string_view)>& message = {}) const;
};

}

#endif

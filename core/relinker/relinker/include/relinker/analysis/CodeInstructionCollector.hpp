#ifndef RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP
#define RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP

#include <domain/Types.hpp>
#include <set>

namespace Relinker {

class CodeInstructionCollector {
public:
    std::set<Domain::VirtualAddress> Collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers) const;
};

}

#endif

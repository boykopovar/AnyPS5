#ifndef RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP
#define RELINKER_CODEINSTRUCTIONCOLLECTOR_HPP

#include <domain/Types.hpp>
#include <set>
#include <functional>
#include <string_view>
#include <vector>

namespace Relinker {

class CodeInstructionCollector {
public:
    std::vector<Domain::VirtualAddress> Collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}, std::set<Domain::VirtualAddress>* entryRoots = nullptr, const std::function<void(std::string_view)>& message = {}, std::vector<Domain::CodeRange>* tailReachableRanges = nullptr, const std::vector<Domain::VirtualAddress>& extraEntries = {}) const;
    std::vector<Domain::CodeRange> CollectTailReachableRanges(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}) const;

    std::set<Domain::VirtualAddress> CollectRewriteAnchors(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}) const;

private:
    std::vector<Domain::VirtualAddress> collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress, std::set<Domain::VirtualAddress>* entryRoots, const std::function<void(std::string_view)>& message, std::vector<Domain::CodeRange>* tailReachableRanges, bool allowIdenticalDynamicTags, bool forceAuthoritative, const std::vector<Domain::VirtualAddress>& extraEntries) const;
};

}

#endif

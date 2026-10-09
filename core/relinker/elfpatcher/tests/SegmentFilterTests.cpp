#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <iostream>
#include <stdexcept>

namespace {

using namespace Elfpatcher;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Domain::ProgramHeader segment(std::uint32_t type, std::uint64_t mappedAddress = 0, std::uint64_t fileSize = 0) {
    Domain::ProgramHeader header{};
    header.Type = type;
    header.MappedAddress = mappedAddress;
    header.FileSize = fileSize;
    return header;
}

}

int main() {
    try {
        const SegmentFilter filter;

        require(!filter.ShouldSkip(segment(PT_LOAD)), "PT_LOAD must be kept");
        require(filter.ShouldSkip(segment(PT_DYNAMIC)), "PT_DYNAMIC must be skipped");

        require(filter.ShouldSkip(segment(PT_SCE_DYNLIBDATA)), "PT_SCE_DYNLIBDATA must be skipped");
        require(filter.ShouldSkip(segment(PT_OS_RELRO)), "PT_OS_RELRO must be skipped");
        require(filter.ShouldSkip(segment(PT_LOOS)), "the start of the SCE OS range must be skipped");
        require(filter.ShouldSkip(segment(PT_HIOS)), "the end of the SCE OS range must be skipped");
        require(filter.ShouldSkip(segment(PT_LOOS + 0x1234u)), "an unnamed type inside the SCE OS range must be skipped");

        require(!filter.ShouldSkip(segment(PT_LOOS - 1u)), "a type just below the SCE OS range must be kept");
        require(!filter.ShouldSkip(segment(PT_HIOS + 1u)), "a type just above the SCE OS range must be kept");

        require(!filter.ShouldSkip(segment(PT_OS_PROCPARAM)), "PT_OS_PROCPARAM must be kept even though it is inside the SCE OS range");
        require(!filter.ShouldSkip(segment(PT_GNU_EH_FRAME)), "PT_GNU_EH_FRAME must be kept even though it is inside the SCE OS range");

        require(filter.ShouldSkip(segment(PT_NOTE, 0, 0x40)), "an unmapped non-empty PT_NOTE must be skipped");
        require(!filter.ShouldSkip(segment(PT_NOTE, 0x1000, 0x40)), "a mapped PT_NOTE must be kept");
        require(!filter.ShouldSkip(segment(PT_NOTE, 0, 0)), "an empty PT_NOTE must be kept");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/general/ElfConstants.hpp>

namespace Elfpatcher {

bool SegmentFilter::_isSceSpecificSegment(const std::uint32_t type) const {
    return type == PT_SCE_DYNLIBDATA
        || type == PT_OS_PROCPARAM
        || type == PT_OS_RELRO
        || (type >= PT_LOOS && type <= PT_HIOS);
}

bool SegmentFilter::ShouldSkip(const Domain::ProgramHeader& ph) const {
    if (_isSceSpecificSegment(ph.Type)) return true;
    if (ph.Type == PT_DYNAMIC) return true;
    if (ph.Type == PT_NOTE && ph.MappedAddress == 0 && ph.FileSize > 0) return true;
    return false;
}

}

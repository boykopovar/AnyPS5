#include <elfpatcher/filtering/SegmentFilter.hpp>
#include <elfpatcher/domain/ElfConstants.hpp>

namespace Elfpatcher {

bool SegmentFilter::_isSceSpecificSegment(const std::uint32_t type) const {
    return type == PT_SCE_DYNLIBDATA
        || type == PT_OS_PROCPARAM
        || type == PT_OS_RELRO
        || (type >= 0x61000000 && type <= 0x6fffffff);
}

bool SegmentFilter::_isNullPageLoad(const Domain::ProgramHeader& ph) const {
    return ph.Type == PT_LOAD && ph.MappedAddress == 0;
}

bool SegmentFilter::ShouldSkip(const Domain::ProgramHeader& ph) const {
    if (_isSceSpecificSegment(ph.Type)) return true;
    if (ph.Type == PT_DYNAMIC) return true;
    if (_isNullPageLoad(ph)) return true;
    if (ph.Type == PT_NOTE && ph.MappedAddress == 0 && ph.FileSize > 0) return true;
    return false;
}

}

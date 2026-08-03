#ifndef RELINKER_ISYSCALLSCANNER_HPP
#define RELINKER_ISYSCALLSCANNER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

class ISyscallScanner {
public:
    virtual ~ISyscallScanner() = default;

    virtual void ScanCodeSectionForSyscalls(
        const std::vector<std::uint8_t>& codeSection,
        FileByteOffset codeSectionOffset,
        FileByteOffset codeSectionSize
    ) = 0;
};

}

#endif // RELINKER_ISYSCALLSCANNER_HPP

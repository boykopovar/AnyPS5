#ifndef RELINKER_ISYSCALLSCANNER_HPP
#define RELINKER_ISYSCALLSCANNER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

class ISyscallScanner {
public:
    virtual ~ISyscallScanner() = default;

    virtual void ScanCodeSectionForSyscalls(
        const std::vector<std::uint8_t>& CodeSection,
        FileByteOffset CodeSectionOffset,
        FileByteOffset CodeSectionSize
    ) = 0;
};

}

#endif // RELINKER_ISYSCALLSCANNER_HPP

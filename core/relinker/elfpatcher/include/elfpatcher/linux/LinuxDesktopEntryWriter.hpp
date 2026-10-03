#ifndef ELFPATCHER_LINUX_LINUXDESKTOPENTRYWRITER_HPP
#define ELFPATCHER_LINUX_LINUXDESKTOPENTRYWRITER_HPP

#include <filesystem>

namespace Elfpatcher::Linux {

class LinuxDesktopEntryWriter {
public:
    std::filesystem::path Write(const std::filesystem::path& sceSysDirectory, const std::filesystem::path& executablePath) const;
};

}

#endif

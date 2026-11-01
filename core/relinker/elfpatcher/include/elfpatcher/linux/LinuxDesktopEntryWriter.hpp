#ifndef ELFPATCHER_LINUX_LINUXDESKTOPENTRYWRITER_HPP
#define ELFPATCHER_LINUX_LINUXDESKTOPENTRYWRITER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Elfpatcher::Linux {

struct LinuxDesktopEntry {
    std::filesystem::path IconPath;
    std::vector<std::uint8_t> Icon;
    std::filesystem::path EntryPath;
    std::string Text;
};

class LinuxDesktopEntryWriter {
public:
    std::optional<LinuxDesktopEntry> Prepare(const std::filesystem::path& sceSysDirectory, const std::filesystem::path& executablePath) const;
    std::filesystem::path Write(const LinuxDesktopEntry& entry) const;
};

}

#endif

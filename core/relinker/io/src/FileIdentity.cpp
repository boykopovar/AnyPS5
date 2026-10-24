#include <io/FileIdentity.hpp>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#endif

namespace Io {

#ifdef _WIN32
namespace {

using Identity = std::pair<std::uint64_t, std::array<std::uint8_t, 16>>;

Identity FileIdentity(const std::filesystem::path& path) {
    const auto handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::filesystem::filesystem_error("Cannot open file identity", path, std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    Identity identity{};
    FILE_ID_INFO idInformation{};
    BY_HANDLE_FILE_INFORMATION information{};
    bool read = GetFileInformationByHandleEx(handle, FileIdInfo, &idInformation, sizeof(idInformation)) != 0;
    if (read) {
        identity.first = idInformation.VolumeSerialNumber;
        std::copy(std::begin(idInformation.FileId.Identifier), std::end(idInformation.FileId.Identifier), identity.second.begin());
    } else {
        read = GetFileInformationByHandle(handle, &information) != 0;
        identity.first = information.dwVolumeSerialNumber;
        const std::uint64_t index = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow;
        std::memcpy(identity.second.data(), &index, sizeof(index));
    }
    const auto error = GetLastError();
    CloseHandle(handle);
    if (!read) throw std::filesystem::filesystem_error("Cannot read file identity", path, std::error_code(static_cast<int>(error), std::system_category()));
    return identity;
}

}
#endif

bool SameFile(const std::filesystem::path& first, const std::filesystem::path& second) {
#ifdef _WIN32
    return FileIdentity(first) == FileIdentity(second);
#else
    return std::filesystem::equivalent(first, second);
#endif
}

}

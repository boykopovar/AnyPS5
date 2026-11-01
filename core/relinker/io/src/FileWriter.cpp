#include <io/FileWriter.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <filesystem>
#include <fstream>
#include <system_error>
#ifndef _WIN32
#include <cerrno>
#include <random>
#include <string_view>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Io {

namespace {

bool writeFile(const std::filesystem::path& file, const std::string& path, const std::ios::openmode mode, const char* data, const std::size_t size) {
    std::ofstream f(file, mode);
    if (!f)
        return false;
    f.write(data, static_cast<std::streamsize>(size));
    f.close();
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
    return true;
}

#ifndef _WIN32
int createTemporary(const std::filesystem::path& target, const mode_t mode, std::filesystem::path& temporary) {
    constexpr std::string_view characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device device;
    for (int attempt = 0; attempt < 100; ++attempt) {
        std::string suffix = ".anyps5-";
        for (int index = 0; index < 6; ++index)
            suffix += characters[device() % characters.size()];
        temporary = target.parent_path() / suffix;
        const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
        if (descriptor >= 0 || errno != EEXIST)
            return descriptor;
    }
    return -1;
}

bool writeAll(const int descriptor, const char* data, std::size_t size) {
    while (size != 0) {
        const auto written = write(descriptor, data, size);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

bool replaceFile(std::filesystem::path target, const std::string& path, const char* data, const std::size_t size) {
    std::error_code error;
    auto previous = std::filesystem::symlink_status(target, error);
    if (std::filesystem::is_symlink(previous)) {
        target = std::filesystem::canonical(target, error);
        if (error) return false;
        previous = std::filesystem::symlink_status(target, error);
    }
    const bool regular = std::filesystem::is_regular_file(previous);
    if (!regular && previous.type() != std::filesystem::file_type::not_found)
        return false;
    const auto mode = regular ? static_cast<mode_t>(previous.permissions() & std::filesystem::perms::all) : static_cast<mode_t>(0666);
    std::filesystem::path temporary;
    const int descriptor = createTemporary(target, mode, temporary);
    if (descriptor < 0)
        return false;
    if (regular && fchmod(descriptor, mode) != 0) {
        close(descriptor);
        std::filesystem::remove(temporary, error);
        return false;
    }
    const bool written = writeAll(descriptor, data, size);
    if (close(descriptor) != 0 || !written) {
        std::filesystem::remove(temporary, error);
        throw Domain::RelinkerException("Failed to write file: " + path);
    }
    std::filesystem::rename(temporary, target, error);
    if (!error)
        return true;
    std::filesystem::remove(temporary, error);
    return false;
}
#endif

void writeOutput(const std::string& path, const std::ios::openmode mode, const char* data, const std::size_t size) {
    const auto target = NativePath(path);
#ifndef _WIN32
    if (replaceFile(target, path, data, size))
        return;
#endif
    if (!writeFile(target, path, mode, data, size))
        throw Domain::RelinkerException("Cannot open output file: " + path);
}

}

void FileWriter::Write(const std::string& path, const std::vector<std::uint8_t>& data) {
    writeOutput(path, std::ios::binary, reinterpret_cast<const char*>(data.data()), data.size());
}

void FileWriter::Write(const std::string& path, const std::string& content) {
    writeOutput(path, std::ios::out, content.data(), content.size());
}

}

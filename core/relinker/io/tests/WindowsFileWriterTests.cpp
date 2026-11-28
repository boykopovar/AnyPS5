#include <io/FileWriter.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.is_open(), "Cannot read test output: " + Io::Utf8Path(path));
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void requireStageCleanup(const std::filesystem::path& directory) {
    std::vector<std::string> stages;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(".anyps5-")) stages.push_back(name);
    }
    std::sort(stages.begin(), stages.end());
    require(stages == std::vector<std::string>{".anyps5-leftover"}, "A write leaked or removed a staging directory");
    require(readFile(directory / ".anyps5-leftover" / "previous") == "unrelated", "A write changed an earlier staging directory");
}

template<typename TValue>
void requireIndependentReplacement(const std::filesystem::path& directory, const TValue& data, const std::string& expected) {
    const auto output = directory / "independent.bin";
    const auto alias = directory / "independent-alias.bin";
    const std::string previous(4096, 'p');
    std::ofstream(output, std::ios::binary) << previous;
    std::filesystem::create_hard_link(output, alias);
    const auto raw = CreateFileW(output.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(raw != INVALID_HANDLE_VALUE, "Cannot retain a shared old output handle");
    const std::unique_ptr<void, decltype(&CloseHandle)> held(raw, CloseHandle);
    Io::FileWriter().Write(Io::Utf8Path(output), data);
    require(readFile(output) == expected, "Replacement output has incorrect bytes");
    require(readFile(alias) == previous, "Replacement changed the previous file through a hard link");
    std::string retained(previous.size() + 1, '\0');
    DWORD count = 0;
    require(ReadFile(raw, retained.data(), static_cast<DWORD>(retained.size()), &count, nullptr) != 0,
            "Cannot read the retained old output handle");
    retained.resize(count);
    require(retained == previous, "Replacement changed the previous file through an open handle");
    requireStageCleanup(directory);
}

template<typename TValue>
void requireMetadata(const std::filesystem::path& directory, const TValue& data, const std::string& expected) {
    const auto output = directory / "metadata.bin";
    const auto stream = std::filesystem::path(output.wstring() + L":anyps5-test");
    std::ofstream(output, std::ios::binary) << "previous";
    std::ofstream(stream, std::ios::binary) << "retained stream";
    const FILETIME created{0x12345678u, 0x01cc0000u};
    {
        const auto raw = CreateFileW(output.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(raw != INVALID_HANDLE_VALUE, "Cannot open creation-time fixture");
        const std::unique_ptr<void, decltype(&CloseHandle)> held(raw, CloseHandle);
        require(SetFileTime(raw, &created, nullptr, nullptr) != 0, "Cannot set fixture creation time");
    }
    Io::FileWriter().Write(Io::Utf8Path(output), data);
    require(readFile(output) == expected, "Metadata replacement has incorrect bytes");
    require(readFile(stream) == "retained stream", "Replacement removed a named stream");
    Io::FileWriter().Write(Io::Utf8Path(stream), data);
    require(readFile(stream) == expected && readFile(output) == expected, "Writing a named stream changed the main file");
    {
        const auto raw = CreateFileW(output.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(raw != INVALID_HANDLE_VALUE, "Cannot open replaced metadata fixture");
        const std::unique_ptr<void, decltype(&CloseHandle)> held(raw, CloseHandle);
        FILETIME actual{};
        require(GetFileTime(raw, &actual, nullptr, nullptr) != 0, "Cannot read replaced creation time");
        require(CompareFileTime(&created, &actual) == 0, "Replacement changed the creation time");
    }
    requireStageCleanup(directory);
}

template<typename TValue>
void requireSymlink(const std::filesystem::path& directory, const TValue& data, const std::string& expected) {
    const auto target = directory / "symlink-target.bin";
    const auto link = directory / "symlink.bin";
    std::ofstream(target, std::ios::binary) << "previous";
    if (!CreateSymbolicLinkW(link.c_str(), L"symlink-target.bin", 2)) {
        const auto error = GetLastError();
        if (error == ERROR_PRIVILEGE_NOT_HELD || error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED) {
            std::cout << "Symbolic-link case skipped: " << error << '\n';
            return;
        }
        throw std::runtime_error("Cannot create symbolic-link fixture: " + std::to_string(error));
    }
    Io::FileWriter().Write(Io::Utf8Path(link), data);
    const auto attributes = GetFileAttributesW(link.c_str());
    require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT), "Replacement removed the symbolic link");
    require(readFile(target) == expected, "Replacement did not update the symbolic-link target");
    requireStageCleanup(directory);
    require(DeleteFileW(link.c_str()) != 0, "Cannot remove the symbolic-link fixture");
}

class ReadOnlyFile {
    std::filesystem::path path;
    DWORD attributes;

public:
    explicit ReadOnlyFile(const std::filesystem::path& file) : path(file), attributes(GetFileAttributesW(path.c_str())) {
        require(attributes != INVALID_FILE_ATTRIBUTES, "Cannot read test file attributes");
        require(SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != 0, "Cannot protect test output");
    }
    ~ReadOnlyFile() { SetFileAttributesW(path.c_str(), attributes); }
    ReadOnlyFile(const ReadOnlyFile&) = delete;
    ReadOnlyFile& operator=(const ReadOnlyFile&) = delete;
};

template<typename TValue>
void requireOpenFailure(const std::filesystem::path& path, const TValue& data) {
    const auto name = Io::Utf8Path(path);
    try {
        Io::FileWriter().Write(name, data);
    } catch (const Domain::RelinkerException& error) {
        require(error.what() == "Cannot open output file: " + name, "Unexpected output error: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Writing an unavailable output reported success");
}

template<typename TValue>
void run(const std::filesystem::path& directory, const TValue& data, const std::string& expected) {
    Io::FileWriter writer;
    std::filesystem::create_directory(directory / ".anyps5-leftover");
    std::ofstream(directory / ".anyps5-leftover" / "previous", std::ios::binary) << "unrelated";
    const auto output = directory / std::filesystem::path(u8"Gr\u00fc\u00dfe-\u65e5\u672c.bin");
    const auto name = Io::Utf8Path(output);
    writer.Write(name, data);
    require(readFile(output) == expected, "A new output has incorrect bytes");

    const std::string previous(4096, 'x');
    std::ofstream(output, std::ios::binary) << previous;
    writer.Write(name, data);
    require(readFile(output) == expected, "Replacing an output left old bytes or changed the data");
    writer.Write(name, TValue{});
    require(readFile(output).empty(), "An empty output retained old bytes");

    std::ofstream(output, std::ios::binary) << previous;
    {
        const auto raw = CreateFileW(output.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(raw != INVALID_HANDLE_VALUE, "Cannot lock test output");
        const std::unique_ptr<void, decltype(&CloseHandle)> locked(raw, CloseHandle);
        requireOpenFailure(output, data);
        require(readFile(output) == previous, "A sharing violation changed the previous output");
        requireStageCleanup(directory);
    }
    {
        const ReadOnlyFile readOnly(output);
        requireOpenFailure(output, data);
        require(readFile(output) == previous, "A read-only output was changed");
        requireStageCleanup(directory);
    }
    writer.Write(name, data);
    require(readFile(output) == expected, "Writing after releasing output protection failed");

    const auto missing = directory / "missing" / "output.bin";
    requireOpenFailure(missing, data);
    require(!std::filesystem::exists(missing.parent_path()), "A failed write created a parent directory");
    requireOpenFailure(directory, data);
    require(std::filesystem::is_directory(directory), "A failed write replaced a directory");
    requireStageCleanup(directory);
    requireIndependentReplacement(directory, data, expected);
    requireMetadata(directory, data, expected);
    requireSymlink(directory, data, expected);
    const auto longest = std::filesystem::path(L"\\\\?\\" + directory.wstring()) / std::string(255, 'a');
    writer.Write(Io::Utf8Path(longest), data);
    require(readFile(longest) == expected, "A maximum-length filename could not be written");
    writer.Write(Io::Utf8Path(longest), data);
    require(readFile(longest) == expected, "A maximum-length filename could not be replaced");
    require(std::filesystem::remove(longest), "Cannot remove the extended-path filename fixture");
    requireStageCleanup(directory);
}

}

int main(int argc, char** argv) {
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-windows-file-writer-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) /
        std::filesystem::path(u8"\u00dcbertragung-\u65e5\u672c");
    try {
        require(argc == 2, "Expected binary or text");
        std::filesystem::create_directories(directory);
        const std::string mode(argv[1]);
        if (mode == "binary") {
            const std::vector<std::uint8_t> data{0x00, 0x0a, 0x0d, 0x1a, 0x80, 0xff};
            run(directory, data, std::string(data.begin(), data.end()));
            std::vector<std::uint8_t> large(131073);
            for (std::size_t index = 0; index < large.size(); ++index) large[index] = static_cast<std::uint8_t>(index);
            const auto output = directory / "large.bin";
            Io::FileWriter().Write(Io::Utf8Path(output), large);
            require(readFile(output) == std::string(large.begin(), large.end()), "A multi-chunk binary output has incorrect bytes");
        } else if (mode == "text") {
            const std::string data("first\n\0last\n", 12);
            run(directory, data, std::string("first\r\n\0last\r\n", 14));
            std::string large(65535, 'x');
            large.append("\r\n\0tail\n", 8);
            const auto reference = directory / "large-reference.txt";
            {
                std::ofstream file(reference);
                file.write(large.data(), static_cast<std::streamsize>(large.size()));
                file.close();
                require(static_cast<bool>(file), "Cannot write the reference text fixture");
            }
            const auto output = directory / "large.txt";
            Io::FileWriter().Write(Io::Utf8Path(output), large);
            require(readFile(output) == readFile(reference), "A chunk-boundary text output differs from native text I/O");
        } else {
            throw std::runtime_error("Expected binary or text");
        }
        requireStageCleanup(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory.parent_path(), ignored);
        return 1;
    }
    std::filesystem::remove_all(directory.parent_path());
    return 0;
}

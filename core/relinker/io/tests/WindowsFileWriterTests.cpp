#include <io/FileWriter.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <windows.h>
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
    require(file.is_open(), "Cannot read test output");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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
    }
    {
        const ReadOnlyFile readOnly(output);
        requireOpenFailure(output, data);
        require(readFile(output) == previous, "A read-only output was changed");
    }
    writer.Write(name, data);
    require(readFile(output) == expected, "Writing after releasing output protection failed");

    const auto missing = directory / "missing" / "output.bin";
    requireOpenFailure(missing, data);
    require(!std::filesystem::exists(missing.parent_path()), "A failed write created a parent directory");
    requireOpenFailure(directory, data);
    require(std::filesystem::is_directory(directory), "A failed write replaced a directory");
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
        } else if (mode == "text") {
            const std::string data("first\n\0last\n", 12);
            run(directory, data, std::string("first\r\n\0last\r\n", 14));
        } else {
            throw std::runtime_error("Expected binary or text");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory.parent_path(), ignored);
        return 1;
    }
    std::filesystem::remove_all(directory.parent_path());
    return 0;
}

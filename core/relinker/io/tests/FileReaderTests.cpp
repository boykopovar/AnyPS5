#include <io/FileReader.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void requireError(const TAction& action, const std::string& message) {
    try {
        action();
    } catch (const Domain::RelinkerException& error) {
        require(error.what() == message, "Unexpected prefix-read error: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Invalid prefix read was accepted: " + message);
}

void prefixes(const std::filesystem::path& path, const Bytes& expected) {
    const Io::FileReader reader;
    const auto name = Io::Utf8Path(path);
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
    for (const std::size_t limit : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{4095},
            std::size_t{4096}, std::size_t{4097}, std::size_t{8191}, std::size_t{8192}, std::size_t{8193},
            expected.size(), expected.size() + 1, maximum}) {
        const auto count = std::min(expected.size(), limit);
        require(reader.ReadPrefix(name, limit) == Bytes(expected.begin(), expected.begin() + count),
            "Incorrect file prefix for limit " + std::to_string(limit));
    }
}

void fileContents(const std::filesystem::path& directory) {
    const auto path = directory / Io::NativePath("prefix-\xc3\xa9-\xe6\x97\xa5.bin");
    for (const std::size_t size : {std::size_t{0}, std::size_t{3}, std::size_t{4096}, std::size_t{4097},
            std::size_t{8192}, std::size_t{10003}}) {
        Bytes expected(size);
        for (std::size_t index = 0; index < size; ++index)
            expected[index] = static_cast<std::uint8_t>(index * 37 + 13);
        std::ofstream output(path, std::ios::binary);
        if (!expected.empty())
            output.write(reinterpret_cast<const char*>(expected.data()), static_cast<std::streamsize>(expected.size()));
        output.close();
        require(static_cast<bool>(output), "Cannot prepare the prefix-read fixture");
        prefixes(path, expected);
        require(Io::FileReader().Read(Io::Utf8Path(path)) == expected, "Prefix reads changed the input file");
    }
}

void invalidRequests(const std::filesystem::path& directory) {
    const Io::FileReader reader;
    const auto missing = Io::Utf8Path(directory / "missing.bin");
    const auto folder = Io::Utf8Path(directory);
    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{4097}}) {
        requireError([&] { reader.ReadPrefix(missing, size); }, "Cannot open file: " + missing);
        requireError([&] { reader.ReadPrefix(folder, size); }, "Cannot open file: " + folder);
    }
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
    if (maximum < std::numeric_limits<std::size_t>::max()) {
        requireError([&] { reader.ReadPrefix(missing, maximum + 1); }, "File prefix is too large: " + missing);
    }
}

void zeroReportedFileSize() {
#ifdef __linux__
    const std::filesystem::path path = "/proc/self/cmdline";
    require(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) == 0,
        "Expected a regular proc file with a zero reported size");
    std::ifstream input(path, std::ios::binary);
    const Bytes expected{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    require(!expected.empty(), "Cannot read the proc prefix fixture");
    prefixes(path, expected);
#endif
}

}

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-file-reader-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directory(directory);
        fileContents(directory);
        invalidRequests(directory);
        zeroReportedFileSize();
        std::filesystem::remove_all(directory);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        return 1;
    }
    std::cout << "File prefix-read tests passed\n";
    return 0;
}

#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using namespace Testing;

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t SmallSize = 3;
constexpr std::size_t LargeSize = 65536;
const std::string FullDevice = "/dev/full";
const std::string NullDevice = "/dev/null";

std::string ReadContents(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(static_cast<bool>(file), "cannot read back " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

template<typename TValue>
void RequireFullDeviceFailure(const TValue& data) {
    Io::FileWriter writer;

    RequireThrowsWithMessage<Domain::RelinkerException>([&] { writer.Write(FullDevice, data); },
                                                        "Failed to write file: " + FullDevice, "write to /dev/full");
}

const Case binaryToNullDevice{"FileWriter_BinaryDataToNullDevice_Succeeds", [] {
    Io::FileWriter writer;

    writer.Write(NullDevice, Bytes(SmallSize, 0x41));
    writer.Write(NullDevice, Bytes(LargeSize, 0x41));
}};

const Case smallBinaryToFullDevice{"FileWriter_SmallBinaryDataToFullDevice_ThrowsWriteFailure", [] {
    RequireFullDeviceFailure(Bytes(SmallSize, 0x41));
}};

const Case largeBinaryToFullDevice{"FileWriter_LargeBinaryDataToFullDevice_ThrowsWriteFailure", [] {
    RequireFullDeviceFailure(Bytes(LargeSize, 0x41));
}};

const Case binaryToFile{"FileWriter_BinaryDataToFile_WritesExactBytes", [] {
    const TemporaryDirectory directory;
    const auto path = directory.Path() / "binary.bin";
    const Bytes data{0x00, 0x0A, 0x0D, 0x41, 0xFF};
    Io::FileWriter writer;

    writer.Write(path.string(), data);

    RequireEqual(ReadContents(path), std::string(data.begin(), data.end()), "written bytes");
}};

const Case textToNullDevice{"FileWriter_TextToNullDevice_Succeeds", [] {
    Io::FileWriter writer;

    writer.Write(NullDevice, std::string(SmallSize, 'A'));
    writer.Write(NullDevice, std::string(LargeSize, 'A'));
}};

const Case smallTextToFullDevice{"FileWriter_SmallTextToFullDevice_ThrowsWriteFailure", [] {
    RequireFullDeviceFailure(std::string(SmallSize, 'A'));
}};

const Case largeTextToFullDevice{"FileWriter_LargeTextToFullDevice_ThrowsWriteFailure", [] {
    RequireFullDeviceFailure(std::string(LargeSize, 'A'));
}};

const Case textToFile{"FileWriter_TextToFile_WritesExactContent", [] {
    const TemporaryDirectory directory;
    const auto path = directory.Path() / "text.txt";
    const std::string content = "first line\nsecond line\n";
    Io::FileWriter writer;

    writer.Write(path.string(), content);

    RequireEqual(ReadContents(path), content, "written text");
}};

} // namespace

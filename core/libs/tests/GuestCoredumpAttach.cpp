#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/AppMetadata/include/ParamSfoParser.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
alignas(16) std::array<std::byte, 0x40> process{};
constexpr int InvalidArgument = static_cast<int>(0x81180000u);
constexpr int NotInCoredumpHandler = static_cast<int>(0x81180003u);
constexpr uint32_t UserFileTruncateIfExceedLimit = 0x80000000u;
bool handlerRan = false;
int handlerContext = 0;

int APS5_VABI Handler(void*) {
    handlerRan = true;
    return 0;
}

void Require(bool value) { if (!value) std::abort(); }

template <typename TAttach>
bool Throws(TAttach attach, const char* function) {
    try { attach(); }
    catch (const std::runtime_error& error) { return std::strstr(error.what(), function) != nullptr; }
    return false;
}

struct SfoEntrySpec {
    std::string key;
    std::uint16_t format;
    std::vector<std::uint8_t> data;
    std::uint32_t dataMaxLen;
};

void WriteU16Le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint16_t value) {
    std::memcpy(out.data() + offset, &value, sizeof(value));
}

void WriteU32Le(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
    std::memcpy(out.data() + offset, &value, sizeof(value));
}

SfoEntrySpec Utf8Entry(std::string key, const std::string& value) {
    std::vector<std::uint8_t> data(value.begin(), value.end());
    data.push_back(0);
    return SfoEntrySpec{std::move(key), 0x0204u, data, static_cast<std::uint32_t>(data.size())};
}

SfoEntrySpec IntEntry(std::string key, std::int32_t value) {
    std::vector<std::uint8_t> data(4);
    std::memcpy(data.data(), &value, sizeof(value));
    return SfoEntrySpec{std::move(key), 0x0404u, std::move(data), 4u};
}

std::vector<std::uint8_t> BuildParamSfo(const std::vector<SfoEntrySpec>& entries) {
    constexpr std::size_t HeaderSize = 20;
    constexpr std::size_t EntrySize = 16;
    const std::size_t indexTableEnd = HeaderSize + entries.size() * EntrySize;
    std::vector<std::uint8_t> keyBytes;
    std::vector<std::uint8_t> dataBytes;
    std::vector<std::uint16_t> keyOffsets;
    std::vector<std::uint32_t> dataOffsets;
    for (const auto& entry : entries) {
        keyOffsets.push_back(static_cast<std::uint16_t>(keyBytes.size()));
        keyBytes.insert(keyBytes.end(), entry.key.begin(), entry.key.end());
        keyBytes.push_back(0);
        dataOffsets.push_back(static_cast<std::uint32_t>(dataBytes.size()));
        dataBytes.insert(dataBytes.end(), entry.data.begin(), entry.data.end());
    }
    const std::uint32_t keyTable = static_cast<std::uint32_t>(indexTableEnd);
    const std::uint32_t dataTable = static_cast<std::uint32_t>(indexTableEnd + keyBytes.size());
    std::vector<std::uint8_t> bytes(dataTable + dataBytes.size(), 0);
    bytes[0] = 0;
    bytes[1] = 'P';
    bytes[2] = 'S';
    bytes[3] = 'F';
    WriteU32Le(bytes, 4, 0x00000101u);
    WriteU32Le(bytes, 8, keyTable);
    WriteU32Le(bytes, 12, dataTable);
    WriteU32Le(bytes, 16, static_cast<std::uint32_t>(entries.size()));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::size_t base = HeaderSize + i * EntrySize;
        WriteU16Le(bytes, base, keyOffsets[i]);
        WriteU16Le(bytes, base + 2, entries[i].format);
        WriteU32Le(bytes, base + 4, static_cast<std::uint32_t>(entries[i].data.size()));
        WriteU32Le(bytes, base + 8, entries[i].dataMaxLen);
        WriteU32Le(bytes, base + 12, dataOffsets[i]);
    }
    std::memcpy(bytes.data() + keyTable, keyBytes.data(), keyBytes.size());
    std::memcpy(bytes.data() + dataTable, dataBytes.data(), dataBytes.size());
    return bytes;
}

struct TempParamSfoFile {
    std::filesystem::path path;
    explicit TempParamSfoFile(const std::string& name, const std::vector<std::uint8_t>& bytes)
        : path(std::filesystem::temp_directory_path() / name) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        Require(out.is_open());
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(out.good());
    }
    ~TempParamSfoFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

void ExpectParamSfoThrows(const std::string& name, const std::vector<std::uint8_t>& bytes) {
    bool threw = false;
    {
        const TempParamSfoFile file(name, bytes);
        try {
            (void)parseParamSfo(file.path);
        } catch (const std::runtime_error&) {
            threw = true;
        }
    }
    Require(threw);
}

void VerifyParamSfoParser() {
    {
        const auto bytes = BuildParamSfo({
            IntEntry("ATTRIBUTE", 0x12345678),
            IntEntry("DOWNLOAD_DATA_SIZE", 64),
            Utf8Entry("TITLE", "Test Title"),
            Utf8Entry("TITLE_ID", "TEST12345"),
            IntEntry("USER_DEFINED_PARAM_1", 11),
            IntEntry("USER_DEFINED_PARAM_2", 22),
            IntEntry("USER_DEFINED_PARAM_3", 33),
            IntEntry("USER_DEFINED_PARAM_4", 44),
        });
        const auto parsed = [&] {
            const TempParamSfoFile file("anyps5_paramsfo_valid.sfo", bytes);
            return parseParamSfo(file.path);
        }();
        Require(parsed.title == "Test Title");
        Require(parsed.titleId == "TEST12345");
        Require(parsed.downloadDataSizeMiB == 64u);
        Require(parsed.paramSfoAttribute == 0x12345678u);
        Require(parsed.userDefinedParams[0] == 11);
        Require(parsed.userDefinedParams[1] == 22);
        Require(parsed.userDefinedParams[2] == 33);
        Require(parsed.userDefinedParams[3] == 44);
    }
    {
        auto bytes = BuildParamSfo({Utf8Entry("TITLE", "Test Title"), Utf8Entry("TITLE_ID", "TEST12345")});
        WriteU32Le(bytes, 8, 20u);
        WriteU16Le(bytes, 20, 32u);
        WriteU16Le(bytes, 36, 38u);
        ExpectParamSfoThrows("anyps5_paramsfo_key_before_index.sfo", bytes);
    }
    {
        std::vector<std::uint8_t> bytes(52 + 21 + 15, 0);
        bytes[0] = 0;
        bytes[1] = 'P';
        bytes[2] = 'S';
        bytes[3] = 'F';
        WriteU32Le(bytes, 4, 0x00000101u);
        WriteU32Le(bytes, 8, 73u);
        WriteU32Le(bytes, 12, 52u);
        WriteU32Le(bytes, 16, 2u);
        WriteU16Le(bytes, 20, 0u);
        WriteU16Le(bytes, 22, 0x0204u);
        WriteU32Le(bytes, 24, 11u);
        WriteU32Le(bytes, 28, 11u);
        WriteU32Le(bytes, 32, 0u);
        WriteU16Le(bytes, 36, 6u);
        WriteU16Le(bytes, 38, 0x0204u);
        WriteU32Le(bytes, 40, 10u);
        WriteU32Le(bytes, 44, 10u);
        WriteU32Le(bytes, 48, 11u);
        const char data[] = "Test Title\0TEST12345\0";
        const char keys[] = "TITLE\0TITLE_ID\0";
        std::memcpy(bytes.data() + 52, data, 21);
        std::memcpy(bytes.data() + 73, keys, 15);
        ExpectParamSfoThrows("anyps5_paramsfo_key_after_data.sfo", bytes);
    }
    {
        std::vector<std::uint8_t> bytes(52 + 14 + 22, 0);
        bytes[0] = 0;
        bytes[1] = 'P';
        bytes[2] = 'S';
        bytes[3] = 'F';
        WriteU32Le(bytes, 4, 0x00000101u);
        WriteU32Le(bytes, 8, 52u);
        WriteU32Le(bytes, 12, 66u);
        WriteU32Le(bytes, 16, 2u);
        WriteU16Le(bytes, 20, 0u);
        WriteU16Le(bytes, 22, 0x0204u);
        WriteU32Le(bytes, 24, 11u);
        WriteU32Le(bytes, 28, 11u);
        WriteU32Le(bytes, 32, 1u);
        WriteU16Le(bytes, 36, 6u);
        WriteU16Le(bytes, 38, 0x0204u);
        WriteU32Le(bytes, 40, 10u);
        WriteU32Le(bytes, 44, 10u);
        WriteU32Le(bytes, 48, 12u);
        const char keys[] = "TITLE\0TITLE_ID";
        const char data[] = "\0Test Title\0TEST12345\0";
        std::memcpy(bytes.data() + 52, keys, 14);
        std::memcpy(bytes.data() + 66, data, 22);
        ExpectParamSfoThrows("anyps5_paramsfo_unterminated_key.sfo", bytes);
    }
    {
        auto bytes = BuildParamSfo({Utf8Entry("TITLE", "Test Title"), Utf8Entry("TITLE_ID", "TEST12345")});
        WriteU32Le(bytes, 28, 4u);
        ExpectParamSfoThrows("anyps5_paramsfo_len_gt_maxlen.sfo", bytes);
    }
    {
        auto bytes = BuildParamSfo({Utf8Entry("TITLE", "Test Title"), Utf8Entry("TITLE_ID", "TEST12345")});
        WriteU32Le(bytes, 44, 64u);
        ExpectParamSfoThrows("anyps5_paramsfo_maxlen_past_eof.sfo", bytes);
    }
}
}

int main() {
    const std::uint64_t processSize = process.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x06500000;
    std::memcpy(process.data(), &processSize, sizeof(processSize));
    std::memcpy(process.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    alignas(uint64_t) std::array<unsigned char, 64> region{};
    for (size_t index = 0; index < region.size(); ++index) region[index] = static_cast<unsigned char>(index * 7 + 3);
    const auto original = region;
    alignas(uint64_t) std::array<unsigned char, 160> context{};
    for (size_t index = 0; index < context.size(); ++index) context[index] = static_cast<unsigned char>(index * 5 + 1);
    const auto originalContext = context;

    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(UserFileTruncateIfExceedLimit | 2, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 8) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 8, 144) == NotInCoredumpHandler);
    Require(region == original && context == originalContext);

    Require(sceCoredumpRegisterCoredumpHandler(reinterpret_cast<uint64_t>(&Handler), 16 * 1024, reinterpret_cast<uint64_t>(&handlerContext)) == 0);
    Require(sceCoredumpAttachMemoryRegion(0, region.data(), 1) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(0xFFFFFFFFu, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    int fromThread = 0;
    std::thread([&] { fromThread = sceCoredumpAttachMemoryRegion(3, region.data(), region.size()); }).join();
    Require(fromThread == NotInCoredumpHandler);
    std::thread([&] { fromThread = sceCoredumpGetStopInfoCpu(region.data() + 16, 16); }).join();
    Require(fromThread == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data(), 144) == NotInCoredumpHandler);
    std::thread([&] { fromThread = sceCoredumpGetThreadContextInfo(context.data() + 8, 144); }).join();
    Require(fromThread == NotInCoredumpHandler);
    Require(region == original && context == originalContext);
    Require(!handlerRan);

    Require(sceCoredumpAttachMemoryRegion(1, nullptr, region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), 0) == NotInCoredumpHandler);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), -1) == 0);
    Require(sceCoredumpAttachUserMemoryFile(1, nullptr, region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), 0) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), -1) == 0);
    Require(sceCoredumpGetStopInfoCpu(nullptr, 16) == InvalidArgument);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 1, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 16) == NotInCoredumpHandler);
    for (const size_t size : {size_t{0}, size_t{1}, size_t{15}, size_t{17}, SIZE_MAX})
        Require(sceCoredumpGetStopInfoCpu(region.data() + 16, size) == InvalidArgument);
    Require(sceCoredumpGetThreadContextInfo(nullptr, 144) == InvalidArgument);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 1, 144) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(reinterpret_cast<void*>(UINTPTR_MAX - 7u), 144) == NotInCoredumpHandler);
    for (const size_t size : {size_t{0}, size_t{1}, size_t{143}, size_t{145}, SIZE_MAX})
        Require(sceCoredumpGetThreadContextInfo(context.data(), size) == InvalidArgument);

    for (const auto length : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
        const int expected = length < 0 ? 0 : NotInCoredumpHandler;
        Require(sceCoredumpAttachMemoryRegion(0, nullptr, length) == expected);
        Require(sceCoredumpAttachUserMemoryFile(0, nullptr, length) == expected);
        Require(sceCoredumpAttachMemoryRegion(0xffffffffu, region.data(), length) == expected);
        Require(sceCoredumpAttachUserMemoryFile(0xffffffffu, region.data(), length) == expected);
    }
    Require(sceCoredumpUnregisterCoredumpHandler() == 0);
    Require(sceCoredumpAttachMemoryRegion(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpAttachUserMemoryFile(1, region.data(), region.size()) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoCpu(region.data() + 16, 16) == NotInCoredumpHandler);
    Require(sceCoredumpGetThreadContextInfo(context.data() + 8, 144) == NotInCoredumpHandler);
    Require(region == original && context == originalContext);
    Require(!handlerRan);

    Require(sceCoredumpGetStopInfoGpu_Agc(region.data() + 8, 8) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoGpu_Agc(region.data() + 8, 3) == NotInCoredumpHandler);
    Require(sceCoredumpGetStopInfoGpu_Agc(nullptr, 8) == InvalidArgument);
    Require(sceCoredumpWriteUserString(1, "text", 4) == 0);
    Require(sceCoredumpWriteUserString(0, "", 0) == 0);
    Require(sceCoredumpWriteUserString(1, nullptr, 4) == InvalidArgument);
    Require(sceCoredumpWriteUserString(15, "x", 256) == 0 && sceCoredumpWriteUserString(0, "x", 0) == 0);
    Require(sceCoredumpWriteUserString(16, "x", 1) == InvalidArgument && sceCoredumpWriteUserString(0x80000000u, "x", 1) == InvalidArgument);
    Require(sceCoredumpWriteUserString(1, "x", 257) == InvalidArgument && sceCoredumpWriteUserString(1, "x", -1) == InvalidArgument);
    Require(region == original);
    Require(sceCoredumpConfigDumpMode(1) == 0);
    for (const int mode : {0, 2, -1}) Require(sceCoredumpConfigDumpMode(mode) == InvalidArgument);
    VerifyParamSfoParser();
    return 0;
}

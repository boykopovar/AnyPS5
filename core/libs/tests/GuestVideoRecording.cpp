#include "prx/libSceVideoRecordingP/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int StatusNone = 0;
constexpr int InvalidArgument = static_cast<int>(0x80a80003);
constexpr int NotOpen = static_cast<int>(0x80a80006);
constexpr int NoVideoFormat = static_cast<int>(0x80a80008);
constexpr int MinimumHeap = 0x1000;
constexpr std::uint8_t Sentinel = 0xA5;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VideoRecording: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

VideoRecordingParam RecordingParam(std::uint32_t format) {
    VideoRecordingParam param{};
    param.size = sizeof(param);
    param.duration = 3600;
    param.mode = 0;
    param.affinityMask = 0x1fff;
    param.threadPriority = 700;
    param.option = 1;
    param.format = format;
    return param;
}

int Query(const VideoRecordingParam& param) {
    return sceVideoRecordingQueryMemSize(&param);
}

constexpr std::int32_t SupportNeoMode = 0x00800000;
constexpr std::int64_t HddInvalidArgument = static_cast<std::int64_t>(InvalidArgument);

struct SizeRow {
    std::uint32_t sdk;
    std::uint32_t duration;
    int expected;
};

struct HddRow {
    std::uint32_t duration;
    std::int64_t expected;
};

const std::vector<SizeRow> BaseSizes{
    {0x01000000, 0, 0x62680}, {0x01000000, 100, 0x71d60}, {0x01000000, 3600, 0x28de00},
    {0x01ffffff, 0, 0x62680}, {0x01ffffff, 100, 0x71d60}, {0x01ffffff, 3600, 0x28de00},
    {0x02000000, 0, 0x75220}, {0x02000000, 100, 0x84900}, {0x02000000, 3600, 0x2a09a0},
    {0x034fffff, 0, 0x75220}, {0x034fffff, 100, 0x84900}, {0x034fffff, 3600, 0x2a09a0},
    {0x03500000, 0, 0xb5220}, {0x03500000, 100, 0xc4900}, {0x03500000, 3600, 0x2e09a0},
    {0x044fffff, 0, 0xb5220}, {0x044fffff, 100, 0xc4900}, {0x044fffff, 3600, 0x2e09a0},
    {0x04500000, 0, 0xc5220}, {0x04500000, 100, 0xd4900}, {0x04500000, 3600, 0x2f09a0},
    {0x09000000, 0, 0xc5220}, {0x09000000, 100, 0xd4900}, {0x09000000, 3600, 0x2f09a0},
};

const std::vector<SizeRow> NeoSizes{
    {0x01000000, 0, 0x62680}, {0x01000000, 100, 0x71d60}, {0x01000000, 3600, 0x28de00},
    {0x01ffffff, 0, 0x62680}, {0x01ffffff, 100, 0x71d60}, {0x01ffffff, 3600, 0x28de00},
    {0x02000000, 0, 0x75220}, {0x02000000, 100, 0x84900}, {0x02000000, 3600, 0x2a09a0},
    {0x034fffff, 0, 0x75220}, {0x034fffff, 100, 0x84900}, {0x034fffff, 3600, 0x2a09a0},
    {0x03500000, 0, 0xfd220}, {0x03500000, 100, 0x10f7e0}, {0x03500000, 3600, 0x392120},
    {0x044fffff, 0, 0xfd220}, {0x044fffff, 100, 0x10f7e0}, {0x044fffff, 3600, 0x392120},
    {0x04500000, 0, 0x10d220}, {0x04500000, 100, 0x11f7e0}, {0x04500000, 3600, 0x3a2120},
    {0x09000000, 0, 0x10d220}, {0x09000000, 100, 0x11f7e0}, {0x09000000, 3600, 0x3a2120},
};

const std::vector<HddRow> BaseHddSizes{{0, 0x1180000}, {10, 0x1180000}, {11, 0x1340000}, {180, 0x13b00000}};
const std::vector<HddRow> NeoHddSizes{{0, 0x1900000}, {10, 0x1900000}, {11, 0x1b80000}, {180, 0x1c200000}};

VideoRecordingParam2 RecordingParam2(std::uint32_t duration) {
    VideoRecordingParam2 param{};
    param.size = sizeof(param);
    param.affinityMask = 0x1fff;
    param.threadPriority = 700;
    param.duration = duration;
    param.format = 1;
    return param;
}

void CheckSize2Arguments() {
    errno = 71;
    Require(sceVideoRecordingQueryMemSize2(reinterpret_cast<const VideoRecordingParam2*>(0x10)) == InvalidArgument, "QueryMemSize2 accepted an unmapped block");
    Require(sceVideoRecordingQueryHddSize2(reinterpret_cast<const VideoRecordingParam2*>(0x10)) == HddInvalidArgument, "QueryHddSize2 accepted an unmapped block");
    Require(errno == 71, "errno was not preserved across an unmapped size query");
    Require(sceVideoRecordingQueryMemSize2(nullptr) == InvalidArgument, "QueryMemSize2 accepted no parameters");
    Require(sceVideoRecordingQueryHddSize2(nullptr) == HddInvalidArgument, "QueryHddSize2 accepted no parameters");
    for (const std::uint64_t size : {0x0ull, 0x10ull, 0x17ull}) {
        VideoRecordingParam2 param = RecordingParam2(10);
        param.size = size;
        Require(sceVideoRecordingQueryMemSize2(&param) == InvalidArgument, "QueryMemSize2 accepted a block shorter than 0x18 bytes");
        Require(sceVideoRecordingQueryHddSize2(&param) == HddInvalidArgument, "QueryHddSize2 accepted a block shorter than 0x18 bytes");
    }
    for (const std::uint32_t duration : {3601u, 7200u}) {
        const VideoRecordingParam2 param = RecordingParam2(duration);
        Require(sceVideoRecordingQueryMemSize2(&param) == InvalidArgument, "QueryMemSize2 accepted a duration above 3600");
    }
    for (const std::uint32_t duration : {181u, 3600u}) {
        const VideoRecordingParam2 param = RecordingParam2(duration);
        Require(sceVideoRecordingQueryHddSize2(&param) == HddInvalidArgument, "QueryHddSize2 accepted a duration above 180");
    }
}

void CheckSize2(std::array<std::byte, 64>& process, bool neo) {
    for (const SizeRow& row : neo ? NeoSizes : BaseSizes) {
        std::memcpy(process.data() + 0x10, &row.sdk, sizeof(row.sdk));
        VideoRecordingParam2 param = RecordingParam2(row.duration);
        Require(sceVideoRecordingQueryMemSize2(&param) == row.expected, "QueryMemSize2 gave the wrong heap size");
        param.size = 0x18;
        param.mode = 1;
        param.outputMode = 3;
        param.option = 2;
        param.format = 0x99;
        Require(sceVideoRecordingQueryMemSize2(&param) == row.expected, "fields past the duration changed QueryMemSize2");
    }
    for (const HddRow& row : neo ? NeoHddSizes : BaseHddSizes) {
        for (const std::uint32_t sdk : {0x01000000u, 0x09000000u}) {
            std::memcpy(process.data() + 0x10, &sdk, sizeof(sdk));
            VideoRecordingParam2 param = RecordingParam2(row.duration);
            Require(sceVideoRecordingQueryHddSize2(&param) == row.expected, "QueryHddSize2 gave the wrong size");
            param.size = 0x18;
            Require(sceVideoRecordingQueryHddSize2(&param) == row.expected, "a 0x18-byte block changed QueryHddSize2");
        }
    }
}

std::vector<std::uint8_t> ParamSfo(std::int32_t attribute) {
    struct Entry {
        std::string key;
        std::uint16_t format;
        std::vector<std::uint8_t> data;
    };
    const auto text = [](const std::string& key, const std::string& value) {
        std::vector<std::uint8_t> data(value.begin(), value.end());
        data.push_back(0);
        return Entry{key, 0x0204, data};
    };
    std::vector<std::uint8_t> attributeData(4);
    std::memcpy(attributeData.data(), &attribute, 4);
    const std::vector<Entry> entries{text("TITLE", "Example"), text("TITLE_ID", "CUSA00000"), {"ATTRIBUTE", 0x0404, attributeData}};
    const auto put32 = [](std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
        std::memcpy(bytes.data() + offset, &value, 4);
    };
    std::vector<std::uint8_t> keys;
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> bytes(20 + entries.size() * 16);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto entry = 20 + index * 16;
        const auto keyOffset = static_cast<std::uint16_t>(keys.size());
        std::memcpy(bytes.data() + entry, &keyOffset, 2);
        std::memcpy(bytes.data() + entry + 2, &entries[index].format, 2);
        put32(bytes, entry + 4, static_cast<std::uint32_t>(entries[index].data.size()));
        put32(bytes, entry + 8, static_cast<std::uint32_t>(entries[index].data.size()));
        put32(bytes, entry + 12, static_cast<std::uint32_t>(data.size()));
        keys.insert(keys.end(), entries[index].key.begin(), entries[index].key.end());
        keys.push_back(0);
        data.insert(data.end(), entries[index].data.begin(), entries[index].data.end());
    }
    while (keys.size() % 4 != 0) keys.push_back(0);
    std::memcpy(bytes.data(), "\0PSF", 4);
    put32(bytes, 4, 0x0101);
    put32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    put32(bytes, 12, static_cast<std::uint32_t>(bytes.size() + keys.size()));
    put32(bytes, 16, static_cast<std::uint32_t>(entries.size()));
    bytes.insert(bytes.end(), keys.begin(), keys.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

void WriteTitle(const std::filesystem::path& directory, std::int32_t attribute) {
    const auto metadata = directory / "app0" / "sce_sys";
    std::filesystem::create_directories(metadata);
    const auto bytes = ParamSfo(attribute);
    std::ofstream file(metadata / "param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "cannot write the param.sfo fixture");
}

int RunSelf(const char* program, std::string_view mode, const std::filesystem::path& directory) {
    const std::string arguments = "\"" + std::string(program) + "\" " + std::string(mode) + " \"" + directory.string() + "\"";
#ifdef _WIN32
    const std::string command = "\"" + arguments + "\"";
#else
    const std::string command = arguments;
#endif
    return std::system(command.c_str());
}

}

int main(int argc, char** argv) {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "cannot install the process parameters");
    if (argc == 3) {
        std::filesystem::current_path(argv[2]);
        const std::string_view mode = argv[1];
        if (mode != "--neo" && mode != "--base") return 1;
        CheckSize2(process, mode == "--neo");
        return 0;
    }

    Require(sceVideoRecordingGetStatus() == StatusNone, "a recorder that was never opened reported a status");
    Require(sceVideoRecordingGetStatus() == StatusNone, "the status changed between two reads");
    Require(sceVideoRecordingStart() == NotOpen, "the encoder started without a recorder");
    Require(sceVideoRecordingStop() == NotOpen, "the encoder stopped without a recorder");
    Require(sceVideoRecordingClose(0) == NotOpen, "a recorder that was never opened closed");
    Require(sceVideoRecordingClose(1) == NotOpen, "a recorder that was never opened closed with discard");

    std::array<std::uint8_t, 24> query{};
    query.fill(Sentinel);
    Require(Throws([&] { _sceVideoRecordingQueryParam_nid_postfix(reinterpret_cast<VideoRecordingQueryParam*>(query.data())); }), "the default parameters were produced");

    errno = 71;
    Require(sceVideoRecordingQueryMemSize(reinterpret_cast<const VideoRecordingParam*>(0x10)) == InvalidArgument, "an unmapped parameter block was refused");
    Require(errno == 71, "errno was preserved across unmapped query");
    Require(sceVideoRecordingQueryMemSize(nullptr) == InvalidArgument, "a heap size was reported for no parameters");
    for (std::uint32_t format = 0x3; format <= 0x44; ++format) {
        if (format == 0x10 || format == 0x11 || format == 0x28 || format == 0x40 || format == 0x41) continue;
        Require(Query(RecordingParam(format)) == InvalidArgument, "an unknown video format was accepted");
    }
    Require(Query(RecordingParam(0)) == NoVideoFormat, "the option without a video format was accepted");
    for (const std::uint64_t size : {0x0ull, 0x8ull, 0xfull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == InvalidArgument, "a block shorter than its duration and mode was accepted");
    }
    for (const std::uint64_t size : {0x10ull, 0x11ull, 0x18ull, 0x1cull, 0x1dull, 0x20ull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == MinimumHeap, "a block without option and format did not ask for 0x1000 bytes");
    }
    for (const std::uint64_t size : {0x21ull, 0x24ull}) {
        VideoRecordingParam param = RecordingParam(1);
        param.size = size;
        Require(Query(param) == NoVideoFormat, "a block that ends before its format was accepted");
    }
    for (const std::uint32_t duration : {0u, 3601u, 7200u, 0xffffffffu}) {
        VideoRecordingParam param = RecordingParam(1);
        param.duration = duration;
        Require(Query(param) == InvalidArgument, "a duration outside 1 to 3600 was accepted");
    }
    for (const std::uint32_t mode : {2u, 3u}) {
        VideoRecordingParam param = RecordingParam(1);
        param.mode = mode;
        Require(Query(param) == InvalidArgument, "a mode above 1 was accepted");
    }
    for (const std::uint32_t option : {2u, 3u}) {
        VideoRecordingParam param = RecordingParam(1);
        param.option = option;
        Require(Query(param) == InvalidArgument, "an option above 1 was accepted");
        param.format = 0;
        Require(Query(param) == InvalidArgument, "an option above 1 without a format was accepted");
    }
    VideoRecordingParam minimal = RecordingParam(0);
    minimal.option = 0;
    Require(Query(minimal) == MinimumHeap, "a block without option and format did not ask for 0x1000 bytes");
    for (const std::uint32_t value : {0u, 1u, 2u, 3u}) {
        VideoRecordingParam param = RecordingParam(0);
        param.option = 0;
        param.outputMode = static_cast<std::int32_t>(value);
        param.threadPriority = static_cast<std::int32_t>(value);
        param.affinityMask = value;
        param.reserved40 = value;
        param.reserved44 = value;
        Require(Query(param) == MinimumHeap, "fields the heap size does not depend on changed it");
    }

    std::array<std::uint8_t, 0x1000> heap{};
    heap.fill(Sentinel);
    const VideoRecordingParam video = RecordingParam(1);
    VideoRecordingParam refused = RecordingParam(1);
    refused.duration = 0;
    Require(sceVideoRecordingOpen(nullptr, &video, heap.data(), static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without a path");
    errno = 71;
    Require(sceVideoRecordingOpen("clip.mp4", reinterpret_cast<const VideoRecordingParam*>(0x10), heap.data(), static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened with unmapped parameters");
    Require(errno == 71, "errno was preserved across unmapped open");
    Require(sceVideoRecordingOpen("clip.mp4", nullptr, heap.data(), static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without parameters");
    Require(sceVideoRecordingOpen("clip.mp4", &video, nullptr, static_cast<int>(heap.size())) == InvalidArgument, "the recorder opened without a heap");
    Require(sceVideoRecordingOpen("clip.mp4", &video, heap.data(), -1) == InvalidArgument, "the recorder opened with a negative heap size");
    Require(sceVideoRecordingOpen("clip.mp4", &refused, heap.data(), 0x7fffffff) == InvalidArgument, "the recorder opened with parameters QueryMemSize refuses");
    Require(sceVideoRecordingOpen("clip.mp4", &minimal, heap.data(), MinimumHeap - 1) == InvalidArgument, "the recorder opened with a heap smaller than QueryMemSize asks");
    VideoRecordingParam optionOnly = RecordingParam(0);
    Require(sceVideoRecordingOpen("clip.mp4", &optionOnly, heap.data(), static_cast<int>(heap.size())) == NoVideoFormat, "the recorder opened with the option and no format");
    for (const std::uint8_t value : heap) Require(value == Sentinel, "the heap was written");
    Require(sceVideoRecordingGetStatus() == StatusNone, "a failed open changed the status");
    Require(sceVideoRecordingStart() == NotOpen, "a failed open left a recorder to start");
    Require(sceVideoRecordingClose(1) == NotOpen, "a failed open left a recorder to close");

    CheckSize2Arguments();
    const auto titles = std::filesystem::temp_directory_path() / "guest_video_recording_size2";
    std::filesystem::remove_all(titles);
    WriteTitle(titles / "base", 0);
    WriteTitle(titles / "neo", SupportNeoMode);
    Require(RunSelf(argv[0], "--base", titles / "base") == 0, "the size queries failed for a title without Neo mode");
    Require(RunSelf(argv[0], "--neo", titles / "neo") == 0, "the size queries failed for a Neo-mode title");
    std::filesystem::remove_all(titles);

    Require(sceVideoRecordingGetInfo(0xa001, nullptr, 0) == NotOpen, "info 0xa001 validation");
    Require(sceVideoRecordingGetInfo(0xa005, nullptr, 0) == NotOpen, "info 0xa005 validation");
    Require(sceVideoRecordingGetInfo(0, nullptr, 0) == InvalidArgument, "unknown info validation");

    std::array<std::uint8_t, 16> bgra{};
    std::array<std::uint8_t, 6> nv12{};
    Require(sceVideoRecordingCopyBGRAtoNV12(nullptr, bgra.data(), 2, 2) == InvalidArgument, "conversion without a destination");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), nullptr, 2, 2) == InvalidArgument, "conversion without a source");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 0, 2) == InvalidArgument, "conversion without width");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 2, 0) == InvalidArgument, "conversion without height");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 3, 2) == InvalidArgument, "conversion with odd width");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 2, 3) == InvalidArgument, "conversion with odd height");
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 16386, 2) == InvalidArgument, "conversion with a width above the bound");
    errno = 71;
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), reinterpret_cast<const void*>(0x10), 2, 2) == InvalidArgument, "conversion read an unmapped source");
    Require(errno == 71, "errno was preserved across an unmapped source");
    Require(sceVideoRecordingCopyBGRAtoNV12(reinterpret_cast<void*>(0x10), bgra.data(), 2, 2) == InvalidArgument, "conversion wrote an unmapped destination");
    Require(errno == 71, "errno was preserved across an unmapped destination");

    const auto CheckColor = [&](std::uint8_t blue, std::uint8_t green, std::uint8_t red, int luma, int blueDifference, int redDifference) {
        for (std::size_t pixel = 0; pixel < 4; ++pixel) {
            bgra[pixel * 4] = blue;
            bgra[pixel * 4 + 1] = green;
            bgra[pixel * 4 + 2] = red;
            bgra[pixel * 4 + 3] = 0xff;
        }
        nv12.fill(Sentinel);
        Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 2, 2) == 0, "conversion refused a solid colour");
        for (std::size_t pixel = 0; pixel < 4; ++pixel) Require(std::abs(static_cast<int>(nv12[pixel]) - luma) <= 1, "solid colour luma");
        Require(std::abs(static_cast<int>(nv12[4]) - blueDifference) <= 1 && std::abs(static_cast<int>(nv12[5]) - redDifference) <= 1, "solid colour chroma");
    };
    CheckColor(0xff, 0xff, 0xff, 235, 128, 128);
    CheckColor(0x00, 0x00, 0x00, 16, 128, 128);
    CheckColor(0x00, 0x00, 0xff, 63, 102, 240);
    CheckColor(0xff, 0x00, 0x00, 32, 240, 118);

    bgra = {0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff};
    nv12.fill(Sentinel);
    Require(sceVideoRecordingCopyBGRAtoNV12(nv12.data(), bgra.data(), 2, 2) == 0, "conversion refused a checkerboard");
    Require(std::abs(static_cast<int>(nv12[0]) - 63) <= 1 && std::abs(static_cast<int>(nv12[1]) - 32) <= 1, "checkerboard first row luma");
    Require(std::abs(static_cast<int>(nv12[2]) - 32) <= 1 && std::abs(static_cast<int>(nv12[3]) - 63) <= 1, "checkerboard second row luma");
    Require(std::abs(static_cast<int>(nv12[4]) - 171) <= 1 && std::abs(static_cast<int>(nv12[5]) - 179) <= 1, "checkerboard averaged chroma");
    std::puts("VideoRecording tests passed");
    return 0;
}

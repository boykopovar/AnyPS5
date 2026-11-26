#include "prx/libSceContentExport/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int InvalidState = static_cast<int>(0x809D3002u);
constexpr int NotInitialized = static_cast<int>(0x809D3004u);
constexpr int InvalidContentType = static_cast<int>(0x809D3012u);
constexpr int InvalidArgument = static_cast<int>(0x809D3016u);
constexpr int InvalidThumbnailType = static_cast<int>(0x809D3019u);

template <typename TException = std::logic_error, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

int allocator = 0;
char path[1024]{};

void* APS5_VABI MockMalloc(std::size_t, void*) {
    return nullptr;
}

void APS5_VABI MockFree(void*, void*) {}

std::int32_t APS5_VABI DataCallback(const void**, std::size_t*, void*) { return 0; }

int Export(std::int32_t expId) {
    ContentExportParam param{};
    ContentExportCallbackParam content{};
    ContentExportCallbackParam thumbnail{};
    return sceContentExportFromDataWithThumbnail(expId, &param, &content, "image/png", &thumbnail, path, sizeof(path));
}

int ExportData(std::int32_t expId) {
    ContentExportParam param{};
    return sceContentExportFromData(expId, &param, 0, nullptr, nullptr, path, sizeof(path));
}

struct DataChunks {
    int step = 0;
};

std::int32_t APS5_VABI ProvideChunks(const void** data, std::size_t* size, void* userData) {
    auto* chunks = static_cast<DataChunks*>(userData);
    if (chunks->step == 0) {
        *data = "hello";
        *size = 5;
        chunks->step = 1;
        return 0;
    }
    *data = " world";
    *size = 6;
    chunks->step = 2;
    return 1;
}

std::int32_t APS5_VABI InvalidChunks(const void**, std::size_t*, void*) {
    return 2;
}

std::int32_t APS5_VABI NegativeChunks(const void**, std::size_t*, void*) {
    return -1;
}

std::int32_t APS5_VABI SingleChunk(const void** data, std::size_t* size, void* userData) {
    const auto& text = *static_cast<const std::string*>(userData);
    *data = text.data();
    *size = text.size();
    return 1;
}

std::int32_t APS5_VABI InvalidDataChunk(const void** data, std::size_t* size, void*) {
    *data = reinterpret_cast<const void*>(0x10);
    *size = 5;
    return 1;
}

struct OldChunks {
    int step = 0;
};

std::int32_t APS5_VABI ProvideOldChunks(const void** data, std::size_t* size, void* userData) {
    auto* chunks = static_cast<OldChunks*>(userData);
    *data = nullptr;
    *size = 0;
    if (chunks->step == 0) {
        *data = "hello";
        *size = 5;
        chunks->step = 1;
        return 0;
    }
    if (chunks->step == 1) {
        chunks->step = 2;
        return 2;
    }
    chunks->step = 3;
    return 1;
}

int ExportDataCallback(std::int32_t expId) {
    ContentExportParam param{};
    return sceContentExportFromData(expId, &param, 0, &DataCallback, nullptr, path, sizeof(path));
}

void ExportFile(std::int32_t expId) {
    ContentExportParam param{};
    sceContentExportFromFile(expId, &param, "source", path, sizeof(path));
}

void ExportFileWithThumbnail(std::int32_t expId) {
    ContentExportParam param{};
    sceContentExportFromFileWithThumbnail(expId, &param, "source", "thumb", path, sizeof(path));
}

void ExportFileForSf(std::int32_t expId) {
    ContentExportParam param{};
    ContentExportSfParam sfParam{};
    sceContentExportFromFileForSf(expId, &param, &sfParam, "source", "thumb", path, sizeof(path));
}

void ExportFileWithContentIdList(std::int32_t expId) {
    ContentExportParam param{};
    std::int64_t id = 1;
    sceContentExportFromFileWithContentIdList(expId, &param, "source", "thumb", &id, 1, path, sizeof(path));
}

int ExportFileWithTitleIdList(std::int32_t expId) {
    ContentExportParam param{};
    return sceContentExportFromFileWithTitleIdList(expId, &param, "source", "thumb", "CUSA00001", path, sizeof(path));
}

ContentExportParam ExportParam(const char* contentType) {
    ContentExportParam param{};
    std::strncpy(param.content_type, contentType, sizeof(param.content_type) - 1);
    return param;
}

int ExportTitleIds(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList) {
    return sceContentExportFromFileWithTitleIdList(expId, param, sourcePath, thumbnailPath, titleIdList, path, sizeof(path));
}

int ExportWithThumbnail(std::int32_t expId, const ContentExportParam* param, const ContentExportCallbackParam* content, const char* thumbnailType, const ContentExportCallbackParam* thumbnail) {
    return sceContentExportFromDataWithThumbnail(expId, param, content, thumbnailType, thumbnail, path, sizeof(path));
}

}

int main() {
    static std::array<std::byte, 0x40> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x08000000;
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    const auto setSdk = [](std::uint32_t version) { std::memcpy(process.data() + 0x10, &version, sizeof(version)); };
    const auto originalDirectory = std::filesystem::current_path();
    const auto testDirectory = originalDirectory / ("content-export-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(testDirectory));
    std::filesystem::current_path(testDirectory);
    ContentExportInitParam initParam{&MockMalloc, &MockFree, nullptr};
    Require(Throws([] { sceContentExportInit(nullptr); }));
    errno = 71;
    Require(Throws([] { sceContentExportInit(reinterpret_cast<const ContentExportInitParam*>(0x10)); }));
    Require(errno == 71);
    ContentExportInitParam missingMalloc = initParam;
    missingMalloc.mallocFunc = nullptr;
    Require(Throws([&] { sceContentExportInit(&missingMalloc); }));
    ContentExportInitParam missingFree = initParam;
    missingFree.freeFunc = nullptr;
    Require(Throws([&] { sceContentExportInit(&missingFree); }));
    Require(sceContentExportInit(&initParam) == 0);
    Require(Throws([&] { sceContentExportInit(&initParam); }));
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));

    Require(Throws([] { sceContentExportInit2(nullptr); }));
    errno = 71;
    Require(Throws([] { sceContentExportInit2(reinterpret_cast<const ContentExportInitParam2*>(0x10)); }));
    Require(errno == 71);
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));
    Require(sceContentExportStart() == static_cast<int>(0x809D3004u));
    Require(Throws([] { sceContentExportFinish(1); }));
    Require(ExportData(1) == static_cast<int>(0x809D3004u));
    Require(sceContentExportGetProgress(0) == NotInitialized);
    Require(sceContentExportGetProgress(10) == NotInitialized);
    Require(sceContentExportValidateContents(nullptr, 0) == NotInitialized);
    Require(Export(10) == NotInitialized);
    sdkVersion = 0x03000000;
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    Require(ExportData(1) == static_cast<int>(0x809D3016u));
    Require(sceContentExportGetProgress(1) == InvalidArgument);
    Require(ExportFileWithTitleIdList(1) == InvalidArgument);
    Require(Export(1) == InvalidArgument);
    Require(sceContentExportValidateContents(nullptr, 0) == NotInitialized);
    sdkVersion = 0x08000000;
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    Require(Throws([] { ExportFile(1); }));
    Require(Throws([] { ExportFileWithThumbnail(1); }));
    Require(Throws([] { ExportFileForSf(1); }));
    Require(Throws([] { ExportFileWithContentIdList(1); }));
    Require(ExportFileWithTitleIdList(1) == NotInitialized);
    ContentExportInitParam2 missing = param;
    missing.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&missing); }));
    ContentExportInitParam2 reserved = param;
    reserved.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&reserved); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(Throws([&] { sceContentExportInit(&initParam); }));
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));
    Require(sceContentExportInit2(&param) == 0);
    Require(sceContentExportGetProgress(0) == InvalidArgument);
    Require(sceContentExportValidateContents(nullptr, 0) == 0);
    Require(sceContentExportValidateContents(nullptr, 1) == InvalidArgument);
    std::array<std::int64_t, 257> contentIds{};
    Require(sceContentExportValidateContents(contentIds.data(), 257) == InvalidArgument);
    Require(sceContentExportValidateContents(contentIds.data(), 0) == 0);

    std::set<int> sessions;
    for (int i = 0; i < 10; ++i) {
        int session = sceContentExportStart();
        Require(session == i);
        Require(sessions.insert(session).second);
    }
    Require(sceContentExportStart() == static_cast<int>(0x809D3018u));
    Require(sceContentExportTerm() == static_cast<int>(0x809D3003u));
    const ContentExportParam png = ExportParam("image/png");
    const ContentExportParam plain = ExportParam("text/plain");
    ContentExportParam commented = png;
    commented.comment[0] = 'c';
    ContentExportParam unterminated{};
    std::memset(unterminated.content_type, 'a', sizeof(unterminated.content_type));
    const ContentExportCallbackParam callback{5, reinterpret_cast<void*>(&DataCallback), nullptr};
    const ContentExportCallbackParam emptyCallback{0, reinterpret_cast<void*>(&DataCallback), nullptr};
    const ContentExportCallbackParam missingProvider{5, nullptr, nullptr};
    const std::string longestThumbnail(0x400, 't');
    const std::string longThumbnail(0x401, 't');
    const std::string longestList(15000, 'l');
    const std::string longList(15001, 'l');
    Require(sceContentExportGetProgress(9) == 0);
    Require(sceContentExportGetProgress(10) == InvalidArgument);
    Require(sceContentExportGetProgress(-1) == InvalidArgument);
    setSdk(0x05000000);
    Require(sceContentExportGetProgress(9) == InvalidState);
    setSdk(sdkVersion);
    Require(ExportTitleIds(9, nullptr, "/x", "/y", nullptr) == InvalidArgument);
    Require(ExportTitleIds(9, &png, nullptr, "/y", nullptr) == InvalidArgument);
    Require(ExportTitleIds(9, &png, "/x", nullptr, nullptr) == InvalidArgument);
    Require(ExportTitleIds(9, &png, "/x", "", nullptr) == InvalidArgument);
    Require(ExportTitleIds(9, &png, "/x", longThumbnail.c_str(), nullptr) == InvalidArgument);
    Require(ExportTitleIds(9, &png, "/x", "/y", "") == InvalidArgument);
    Require(ExportTitleIds(9, &png, "/x", "/y", longList.c_str()) == InvalidArgument);
    Require(ExportTitleIds(9, &commented, "/x", "/y", nullptr) == InvalidArgument);
    Require(ExportTitleIds(10, &plain, "/x", "/y", nullptr) == InvalidArgument);
    Require(ExportTitleIds(-1, &plain, "/x", "/y", nullptr) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, &callback, "image/jpeg", nullptr) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, nullptr, "image/jpeg", &callback) == InvalidArgument);
    Require(ExportWithThumbnail(9, nullptr, &callback, "image/jpeg", &callback) == InvalidArgument);
    Require(ExportWithThumbnail(9, &commented, &callback, "image/jpeg", &callback) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, &callback, "image/jpeg", &emptyCallback) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, &emptyCallback, "image/jpeg", &callback) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, &callback, "image/jpeg", &missingProvider) == InvalidArgument);
    Require(ExportWithThumbnail(9, &png, &missingProvider, "image/png", &callback) == InvalidArgument);
    Require(ExportWithThumbnail(10, &png, &callback, "image/png", &callback) == InvalidArgument);
    Require(sceContentExportGetProgress(9) == 0);
    Require(ExportWithThumbnail(8, &png, &callback, "image/png", &callback) == InvalidThumbnailType);
    Require(sceContentExportGetProgress(8) == 100);
    setSdk(0x05000000);
    Require(sceContentExportGetProgress(8) == InvalidState);
    setSdk(sdkVersion);
    Require(ExportWithThumbnail(7, &png, &callback, nullptr, &callback) == InvalidThumbnailType);
    Require(ExportWithThumbnail(6, &png, &callback, "image/jpegs", &callback) == InvalidThumbnailType);
    Require(ExportWithThumbnail(5, &plain, &callback, "image/jpeg", &callback) == InvalidContentType);
    Require(sceContentExportGetProgress(5) == 100);
    Require(ExportWithThumbnail(4, &unterminated, &callback, "image/jpeg", &callback) == InvalidContentType);
    Require(ExportTitleIds(9, &plain, "/x", "/y", "[\"CUSA04934\"]") == InvalidContentType);
    Require(sceContentExportGetProgress(9) == 100);
    Require(ExportTitleIds(3, &plain, "/x", longestThumbnail.c_str(), longestList.c_str()) == InvalidContentType);
    Require(ExportTitleIds(2, &unterminated, "/x", "/y", nullptr) == InvalidContentType);
    Require(ExportDataCallback(10) == static_cast<int>(0x809D3016u));
    int first = *sessions.begin();
    ContentExportParam fromData{};
    std::strncpy(fromData.title, "Content Export", sizeof(fromData.title) - 1);
    std::strncpy(fromData.content_type, "image/png", sizeof(fromData.content_type) - 1);
    DataChunks chunks;
    char exported[256]{};
    Require(sceContentExportFromData(first, &fromData, 11, ProvideChunks, &chunks, exported, sizeof(exported)) == 0);
    Require(std::strncmp(exported, "/av_contents/content_tmp/sce_cmgcnt", 35) == 0);
    Require(std::strstr(exported, ".png") != nullptr);
    const auto exportedHostPath = ResolveDiskPath_nid_no_patch(exported);
    std::ifstream exportedFile(exportedHostPath, std::ios::binary);
    const std::string exportedContents((std::istreambuf_iterator<char>(exportedFile)), std::istreambuf_iterator<char>());
    exportedFile.close();
    Require(exportedContents == "hello world");
    std::string largeChunk(8193, 'x');
    largeChunk.back() = 'z';
    Require(sceContentExportFromData(first, &fromData, largeChunk.size(), SingleChunk, &largeChunk, nullptr, 0) == 0);
    {
        std::ifstream largeFile(exportedHostPath, std::ios::binary);
        Require(std::string((std::istreambuf_iterator<char>(largeFile)), std::istreambuf_iterator<char>()) == largeChunk);
    }
    chunks = DataChunks{};
    Require(sceContentExportFromData(first, &fromData, 11, ProvideChunks, &chunks, nullptr, 0) == 0);
    chunks = DataChunks{};
    errno = 71;
    Require(sceContentExportFromData(first, &fromData, 11, ProvideChunks, &chunks, reinterpret_cast<char*>(0x10), sizeof(exported)) == static_cast<int>(0x809D3016u));
    Require(errno == 71);
    chunks = DataChunks{};
    Require(sceContentExportFromData(first, &fromData, 10, ProvideChunks, &chunks, exported, sizeof(exported)) == static_cast<int>(0x809D3016u));
    errno = 71;
    Require(sceContentExportFromData(first, &fromData, 5, InvalidDataChunk, nullptr, nullptr, 0) == static_cast<int>(0x809D3017u));
    Require(errno == 71);
    Require(sceContentExportFromData(first, &fromData, 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D301Bu));
    Require(sceContentExportFromData(first, &fromData, 0, NegativeChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D301Bu));
    ContentExportParam badType = fromData;
    std::strncpy(badType.content_type, "text/plain", sizeof(badType.content_type) - 1);
    Require(sceContentExportFromData(first, &badType, 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D3012u));
    ContentExportParam unterminatedType = fromData;
    std::memset(unterminatedType.content_type, 'a', sizeof(unterminatedType.content_type));
    Require(sceContentExportFromData(first, &unterminatedType, 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D3016u));
    Require(sceContentExportFromData(first, nullptr, 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D3016u));
    errno = 71;
    Require(sceContentExportFromData(first, reinterpret_cast<const ContentExportParam*>(0x10), 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D3016u));
    Require(errno == 71);
    Require(sceContentExportFromData(first, &fromData, 0, nullptr, nullptr, nullptr, 0) == static_cast<int>(0x809D3016u));
    Require(sceContentExportFromData(10, &fromData, 0, InvalidChunks, nullptr, nullptr, 0) == static_cast<int>(0x809D3016u));
    const std::uint32_t oldSdkVersion = 0x01000000;
    std::memcpy(process.data() + 0x10, &oldSdkVersion, sizeof(oldSdkVersion));
    OldChunks oldChunks;
    Require(sceContentExportFromData(first, &fromData, 5, ProvideOldChunks, &oldChunks, nullptr, 0) == 0);
    Require(sceContentExportFromData(first, &fromData, 0, NegativeChunks, nullptr, nullptr, 0) == 0);
    std::memcpy(process.data() + 0x10, &sdkVersion, sizeof(sdkVersion));
    Require(sceContentExportCancel(first) == static_cast<int>(0x809D3002u));
    Require(sceContentExportFinish(first) == 0);
    Require(Throws<std::invalid_argument>([&] { sceContentExportFinish(first); }));
    Require(sceContentExportGetProgress(first) == InvalidArgument);
    Require(Throws<std::invalid_argument>([&] { sceContentExportCancel(first); }));
    Require(Export(first) == InvalidArgument);
    Require(ExportData(first) == static_cast<int>(0x809D3016u));
    Require(Throws<std::invalid_argument>([&] { ExportFile(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileWithThumbnail(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileForSf(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileWithContentIdList(first); }));
    Require(ExportFileWithTitleIdList(first) == InvalidArgument);
    Require(ExportData(0) == static_cast<int>(0x809D3016u));
    Require(Throws<std::invalid_argument>([] { ExportFile(0); }));
    Require(Throws<std::invalid_argument>([] { ExportFileWithThumbnail(0); }));
    Require(ExportData(-1) == static_cast<int>(0x809D3016u));
    Require(ExportData(10) == static_cast<int>(0x809D3016u));
    Require(Throws<std::invalid_argument>([] { ExportFile(-1); }));
    Require(Throws<std::invalid_argument>([] { ExportFileWithThumbnail(-1); }));
    Require(Throws<std::invalid_argument>([] { sceContentExportFinish(0); }));
    Require(Throws<std::invalid_argument>([] { sceContentExportFinish(-1); }));
    sessions.erase(first);
    int reopened = sceContentExportStart();
    Require(reopened == first);
    Require(sessions.insert(reopened).second);
    Require(sceContentExportStart() == static_cast<int>(0x809D3018u));
    Require(sceContentExportFinish(7) == 0);
    Require(sceContentExportFinish(3) == 0);
    Require(sceContentExportStart() == 3);
    Require(sceContentExportStart() == 7);
    for (int session : sessions) Require(sceContentExportFinish(session) == 0);
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportStart() == static_cast<int>(0x809D3004u));
    Require(Throws([&] { sceContentExportFinish(reopened); }));
    Require(ExportData(reopened) == static_cast<int>(0x809D3004u));
    Require(Throws([&] { ExportFile(reopened); }));
    Require(Throws([&] { ExportFileWithThumbnail(reopened); }));
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(testDirectory);
}


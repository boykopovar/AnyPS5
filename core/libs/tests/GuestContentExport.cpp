#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param);
extern "C" int APS5_VABI sceContentExportTerm(void);
extern "C" int APS5_VABI sceContentExportStart(void);
extern "C" int APS5_VABI sceContentExportFinish(int exportId);
extern "C" int APS5_VABI sceContentExportFromFile(int exportId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufLen);
extern "C" int APS5_VABI sceContentExportFromFileWithThumbnail(int exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufLen);

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void WriteAll(const std::filesystem::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary);
    stream << content;
}

int allocator = 0;

constexpr int ErrorFileNotFound = static_cast<int>(0x809D3011u);
constexpr int ErrorNotSupportedFormat = static_cast<int>(0x809D3012u);
constexpr int ErrorLargeTitle = static_cast<int>(0x809D3013u);
constexpr int ErrorNotSupportedThumbnail = static_cast<int>(0x809D3019u);

ContentExportParam MakeParam(const char* contentType) {
    ContentExportParam param{};
    std::strcpy(param.title, "title");
    std::strcpy(param.comment, "comment");
    std::strcpy(param.content_type, contentType);
    return param;
}

}

int main() {
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([] { sceContentExportInit2(nullptr); }));
    Require(Throws([] { sceContentExportTerm(); }));
    Require(Throws([] { sceContentExportStart(); }));
    ContentExportInitParam2 missing = param;
    missing.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&missing); }));
    ContentExportInitParam2 reserved = param;
    reserved.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&reserved); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(sceContentExportTerm() == 0);
    Require(Throws([] { sceContentExportTerm(); }));

    const std::filesystem::path source = ResolvePath_nid_no_patch("/content_export_test_source.bin");
    const std::filesystem::path thumbnail = ResolvePath_nid_no_patch("/content_export_test_thumbnail.png");
    const std::filesystem::path textThumbnail = ResolvePath_nid_no_patch("/content_export_test_thumbnail.txt");
    WriteAll(source, "payload");
    WriteAll(thumbnail, "thumb");
    WriteAll(textThumbnail, "text");

    Require(sceContentExportInit2(&param) == 0);
    const int first = sceContentExportStart();
    const int second = sceContentExportStart();
    Require(first > 0 && second > 0 && first != second);

    char path[64];
    const ContentExportParam image = MakeParam("image/png");
    Require(Throws([&] { sceContentExportFromFile(first + second + 1, &image, "/content_export_test_source.bin", path, sizeof(path)); }));
    Require(Throws([&] { sceContentExportFromFile(first, nullptr, "/content_export_test_source.bin", path, sizeof(path)); }));
    Require(Throws([&] { sceContentExportFromFile(first, &image, nullptr, path, sizeof(path)); }));
    Require(Throws([&] { sceContentExportFromFile(first, &image, "/content_export_test_source.bin", path, 4); }));
    Require(sceContentExportFromFile(first, &image, "/content_export_test_missing.bin", path, sizeof(path)) == ErrorFileNotFound);
    const ContentExportParam unknown = MakeParam("audio/mpeg");
    Require(sceContentExportFromFile(first, &unknown, "/content_export_test_source.bin", path, sizeof(path)) == ErrorNotSupportedFormat);
    ContentExportParam longTitle = image;
    std::memset(longTitle.title, 'a', sizeof(longTitle.title));
    Require(sceContentExportFromFile(first, &longTitle, "/content_export_test_source.bin", path, sizeof(path)) == ErrorLargeTitle);

    Require(sceContentExportFromFile(first, &image, "/content_export_test_source.bin", path, sizeof(path)) == 0);
    Require(std::strncmp(path, "/_ce/", 5) == 0);
    const std::filesystem::path exported = ResolvePath_nid_no_patch(path);
    Require(ReadAll(exported) == "payload");
    Require(ReadAll(source) == "payload");

    char thumbnailed[64];
    const ContentExportParam video = MakeParam("video/mp4");
    Require(sceContentExportFromFileWithThumbnail(second, &video, "/content_export_test_source.bin", "/content_export_test_thumbnail.txt", thumbnailed, sizeof(thumbnailed)) == ErrorNotSupportedThumbnail);
    Require(sceContentExportFromFileWithThumbnail(second, &video, "/content_export_test_source.bin", "/content_export_test_missing.png", thumbnailed, sizeof(thumbnailed)) == ErrorFileNotFound);
    Require(Throws([&] { sceContentExportFromFileWithThumbnail(second, &video, "/content_export_test_source.bin", nullptr, thumbnailed, sizeof(thumbnailed)); }));
    Require(sceContentExportFromFileWithThumbnail(second, &video, "/content_export_test_source.bin", "/content_export_test_thumbnail.png", thumbnailed, sizeof(thumbnailed)) == 0);
    Require(std::strcmp(thumbnailed, path) != 0);
    const std::filesystem::path exportedVideo = ResolvePath_nid_no_patch(thumbnailed);
    Require(ReadAll(exportedVideo) == "payload");
    std::filesystem::path exportedThumbnail = exportedVideo;
    exportedThumbnail.replace_extension(".thumbnail.png");
    Require(ReadAll(exportedThumbnail) == "thumb");

    Require(sceContentExportFinish(first) == 0);
    Require(Throws([&] { sceContentExportFinish(first); }));
    Require(Throws([&] { sceContentExportFromFile(first, &image, "/content_export_test_source.bin", path, sizeof(path)); }));
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportFromFile(second, &video, "/content_export_test_source.bin", path, sizeof(path)); }));
    Require(sceContentExportTerm() == 0);

    std::error_code error;
    std::filesystem::remove(exported, error);
    std::filesystem::remove(exportedVideo, error);
    std::filesystem::remove(exportedThumbnail, error);
    std::filesystem::remove(ResolvePath_nid_no_patch("/_ce"), error);
    std::filesystem::remove(source, error);
    std::filesystem::remove(thumbnail, error);
    std::filesystem::remove(textThumbnail, error);
}

#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <set>
#include <stdexcept>

extern "C" int APS5_VABI sceContentExportAddToFavorites(std::int64_t contentId);
extern "C" int APS5_VABI sceContentExportDataProvideFunction(void);
extern "C" int APS5_VABI sceContentExportFree(void);
extern "C" int APS5_VABI sceContentExportFrom(void);
extern "C" int APS5_VABI sceContentExportFromFileForSf(std::int32_t expId, const ContentExportParam* param, const ContentExportSfParam* sfParam, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufferLength);
extern "C" int APS5_VABI sceContentExportFromFileWithContentIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const std::int64_t* contentIds, std::uint32_t contentCount, char* path, std::size_t pathBufferLength);
extern "C" int APS5_VABI sceContentExportFromFileWithTitleIdList(std::int32_t expId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, const char* titleIdList, char* path, std::size_t pathBufferLength);
extern "C" int APS5_VABI sceContentExportInit(const ContentExportInitParam* param);
extern "C" int APS5_VABI sceContentExportMalloc(void);
extern "C" int APS5_VABI sceContentExportRemoveFromFavorites(std::int64_t contentId);
extern "C" int APS5_VABI sceContentExportRenameContent(std::int64_t contentId, const char* name);
extern "C" int APS5_VABI sceContentExportValidateContents(const std::int64_t* contentIds, std::uint32_t count);

extern "C" int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param);
extern "C" int APS5_VABI sceContentExportTerm(void);
extern "C" int APS5_VABI sceContentExportStart(void);
extern "C" int APS5_VABI sceContentExportFinish(std::int32_t expId);
extern "C" int APS5_VABI sceContentExportGetProgress(std::int32_t expId);
extern "C" int APS5_VABI sceContentExportCancel(std::int32_t expId);
extern "C" int APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t expId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufferLength);

namespace {

void Require(bool value) { if (!value) std::abort(); }

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

void Export(std::int32_t expId) {
    ContentExportParam param{};
    ContentExportCallbackParam content{};
    ContentExportCallbackParam thumbnail{};
    sceContentExportFromDataWithThumbnail(expId, &param, &content, "image/png", &thumbnail, path, sizeof(path));
}

}

int main() {
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([] { sceContentExportInit2(nullptr); }));
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));
    Require(sceContentExportStart() == static_cast<int>(0x809D3004u));
    Require(Throws([] { sceContentExportFinish(1); }));
    ContentExportInitParam2 missing = param;
    missing.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&missing); }));
    ContentExportInitParam2 reserved = param;
    reserved.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&reserved); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));
    Require(sceContentExportInit2(&param) == 0);

    std::set<int> sessions;
    for (int i = 0; i < 10; ++i) {
        int session = sceContentExportStart();
        Require(session == i);
        Require(sessions.insert(session).second);
    }
    Require(sceContentExportStart() == static_cast<int>(0x809D3018u));
    Require(sceContentExportTerm() == static_cast<int>(0x809D3003u));
    int first = *sessions.begin();
    Require(Throws<std::runtime_error>([&] { sceContentExportGetProgress(first); }));
    Require(sceContentExportCancel(first) == static_cast<int>(0x809D3002u));
    Require(Throws<std::runtime_error>([&] { Export(first); }));
    Require(path[0] == 0);
    Require(sceContentExportFinish(first) == 0);
    Require(Throws<std::invalid_argument>([&] { sceContentExportFinish(first); }));
    Require(Throws<std::invalid_argument>([&] { sceContentExportGetProgress(first); }));
    Require(Throws<std::invalid_argument>([&] { sceContentExportCancel(first); }));
    Require(Throws<std::invalid_argument>([&] { Export(first); }));
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
    Require(Throws<std::runtime_error>([] { sceContentExportAddToFavorites(0); }));
    Require(Throws<std::runtime_error>([] { sceContentExportDataProvideFunction(); }));
    Require(Throws<std::runtime_error>([] { sceContentExportFree(); }));
    Require(Throws<std::runtime_error>([] { sceContentExportFrom(); }));
    Require(Throws<std::runtime_error>([] { sceContentExportFromFileForSf(0, nullptr, nullptr, nullptr, nullptr, nullptr, 0); }));
    Require(Throws<std::runtime_error>([] { sceContentExportFromFileWithContentIdList(0, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0); }));
    Require(Throws<std::runtime_error>([] { sceContentExportFromFileWithTitleIdList(0, nullptr, nullptr, nullptr, nullptr, nullptr, 0); }));
    Require(Throws<std::runtime_error>([] { sceContentExportInit(nullptr); }));
    Require(Throws<std::runtime_error>([] { sceContentExportMalloc(); }));
    Require(Throws<std::runtime_error>([] { sceContentExportRemoveFromFavorites(0); }));
    Require(Throws<std::runtime_error>([] { sceContentExportRenameContent(0, nullptr); }));
    Require(Throws<std::runtime_error>([] { sceContentExportValidateContents(nullptr, 0); }));
}


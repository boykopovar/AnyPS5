#include "prx/libSceContentExport/include/Api.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <set>
#include <stdexcept>

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

std::int32_t APS5_VABI DataCallback(const void**, std::size_t*, void*) { return 0; }

void Export(std::int32_t expId) {
    ContentExportParam param{};
    ContentExportCallbackParam content{};
    ContentExportCallbackParam thumbnail{};
    sceContentExportFromDataWithThumbnail(expId, &param, &content, "image/png", &thumbnail, path, sizeof(path));
}

int ExportData(std::int32_t expId) {
    ContentExportParam param{};
    return sceContentExportFromData(expId, &param, 0, nullptr, nullptr, path, sizeof(path));
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

void ExportFileWithTitleIdList(std::int32_t expId) {
    ContentExportParam param{};
    sceContentExportFromFileWithTitleIdList(expId, &param, "source", "thumb", "CUSA00001", path, sizeof(path));
}

}

int main() {
    static std::byte process[64]{};
    Require(ProcessParametersFixture::Install(process, sizeof(process)));
    std::uint32_t sdkVersion = 0x08000000;
    std::memcpy(process + 16, &sdkVersion, sizeof(sdkVersion));
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([] { sceContentExportInit2(nullptr); }));
    Require(sceContentExportTerm() == static_cast<int>(0x809D3004u));
    Require(sceContentExportStart() == static_cast<int>(0x809D3004u));
    Require(Throws([] { sceContentExportFinish(1); }));
    Require(ExportData(1) == static_cast<int>(0x809D3004u));
    sdkVersion = 0x03000000;
    std::memcpy(process + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ExportData(1) == static_cast<int>(0x809D3016u));
    sdkVersion = 0x08000000;
    std::memcpy(process + 16, &sdkVersion, sizeof(sdkVersion));
    Require(Throws([] { ExportFile(1); }));
    Require(Throws([] { ExportFileWithThumbnail(1); }));
    Require(Throws([] { ExportFileForSf(1); }));
    Require(Throws([] { ExportFileWithContentIdList(1); }));
    Require(Throws([] { ExportFileWithTitleIdList(1); }));
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
    Require(Throws<std::runtime_error>([] { ExportDataCallback(0); }));
    Require(ExportDataCallback(10) == static_cast<int>(0x809D3016u));
    int first = *sessions.begin();
    Require(sceContentExportCancel(first) == static_cast<int>(0x809D3002u));
    Require(sceContentExportFinish(first) == 0);
    Require(Throws<std::invalid_argument>([&] { sceContentExportFinish(first); }));
    Require(Throws<std::invalid_argument>([&] { sceContentExportGetProgress(first); }));
    Require(Throws<std::invalid_argument>([&] { sceContentExportCancel(first); }));
    Require(Throws<std::invalid_argument>([&] { Export(first); }));
    Require(ExportData(first) == static_cast<int>(0x809D3016u));
    Require(Throws<std::invalid_argument>([&] { ExportFile(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileWithThumbnail(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileForSf(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileWithContentIdList(first); }));
    Require(Throws<std::invalid_argument>([&] { ExportFileWithTitleIdList(first); }));
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
}


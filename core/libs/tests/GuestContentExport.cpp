#include "SceTypes.hpp"
#include "Decoder/Jpeg.hpp"
#include "prx/libSceContentExport/ContentCatalog.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#endif

extern "C" {
int APS5_VABI sceContentExportInit2(const ContentExportInitParam2*);
int APS5_VABI sceContentExportTerm();
int APS5_VABI sceContentExportStart();
int APS5_VABI sceContentExportFinish(int);
int APS5_VABI sceContentExportFromFile(int, const ContentExportParam*, const char*, char*, std::size_t);
int APS5_VABI sceContentExportFromFileWithThumbnail(int, const ContentExportParam*, const char*, const char*, char*, std::size_t);
int APS5_VABI sceContentExportFromData(int, const ContentExportParam*, std::size_t, ContentExportDataProvideFunction, void*, char*, std::size_t);
}

namespace {

constexpr int Busy = static_cast<int>(0x809D3003);
constexpr int NoInit = static_cast<int>(0x809D3004);
constexpr int NoMemory = static_cast<int>(0x809D3006);
constexpr int FileNotFound = static_cast<int>(0x809D3011);
constexpr int UnsupportedFormat = static_cast<int>(0x809D3012);
constexpr int InvalidParam = static_cast<int>(0x809D3016);
constexpr int ExecutionMax = static_cast<int>(0x809D3018);
constexpr int UnsupportedThumbnail = static_cast<int>(0x809D3019);
constexpr int DataProvideError = static_cast<int>(0x809D301B);

constexpr std::array<std::uint8_t, 70> Png{
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,0x10,0x00,0x00,0x00,0x00,0x81,0xD9,0xFC,
    0x15,0x00,0x00,0x00,0x0D,0x49,0x44,0x41,0x54,0x78,0x9C,0x63,0x10,0x32,0x59,0x7D,
    0x16,0x00,0x03,0x0C,0x01,0xBF,0x6E,0xB9,0xC6,0x5D,0x00,0x00,0x00,0x00,0x49,0x45,
    0x4E,0x44,0xAE,0x42,0x60,0x82
};

void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

template<typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const std::exception&) { return true; }
    return false;
}

struct Allocator {
    std::atomic<int> allocations{0};
    std::atomic<int> releases{0};
    std::atomic<bool> fail{false};
};

void* APS5_VABI Allocate(std::size_t size, void* userData) {
    auto& allocator = *static_cast<Allocator*>(userData);
    if (allocator.fail) return nullptr;
    ++allocator.allocations;
    return std::malloc(size);
}

void APS5_VABI Release(void* pointer, void* userData) {
    ++static_cast<Allocator*>(userData)->releases;
    std::free(pointer);
}

void Write(const std::filesystem::path& path, const void* data, std::size_t size) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    stream.close();
    Require(static_cast<bool>(stream), "fixture write failed");
}

std::vector<std::uint8_t> Read(const char* guest) {
    std::ifstream stream(ResolvePath_nid_no_patch(guest), std::ios::binary);
    Require(static_cast<bool>(stream), "exported path cannot be read");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

ContentExportParam MakeParam(const char* title = "export title", const char* mime = "image/png") {
    ContentExportParam param{};
    std::strcpy(param.title, title);
    std::strcpy(param.content_type, mime);
    return param;
}

struct Chunks {
    std::size_t position = 0;
    int failure = 0;
    bool reenter = false;
    int session = 0;
    bool separateDone = false;
};

int APS5_VABI Provide(void** data, std::size_t* size, void* userData) {
    auto& chunks = *static_cast<Chunks*>(userData);
    if (chunks.reenter) {
        chunks.reenter = false;
        Require(sceContentExportFinish(chunks.session) == Busy, "finish must refuse active callback");
        const auto param = MakeParam();
        std::array<char, 1025> path{};
        Require(sceContentExportFromFile(chunks.session, &param, "/source.png", path.data(), path.size()) == Busy,
            "same session must refuse reentrant export");
        Require(sceContentExportTerm() == Busy, "term must refuse open sessions");
    }
    if (chunks.failure == 1) return -1;
    if (chunks.failure == 2) { *size = Png.size() + 1; *data = const_cast<std::uint8_t*>(Png.data()); return 1; }
    if (chunks.failure == 3) return 0;
    if (chunks.failure == 4) { *size = 1; return 1; }
    if (chunks.failure == 5) { *data = const_cast<std::uint8_t*>(Png.data()); *size = 1; return 1; }
    *size = std::min<std::size_t>(13, Png.size() - chunks.position);
    *data = *size ? const_cast<std::uint8_t*>(Png.data()) + chunks.position : nullptr;
    chunks.position += *size;
    return chunks.position == Png.size() && (!chunks.separateDone || *size == 0) ? 1 : 0;
}

void CheckCatalogTransactions() {
    namespace Catalog = ContentCatalog_nid_no_patch;
    const auto before = Catalog::Read_nid_no_patch();
    Catalog::Transaction_nid_no_patch first;
    Catalog::Transaction_nid_no_patch second;
    const auto entry = [](const auto& transaction) {
        Catalog::Entry item;
        item.id = transaction.Id_nid_no_patch();
        item.title = "transaction";
        item.mimeType = "image/png";
        item.path = transaction.GuestPath_nid_no_patch("content.png");
        item.size = Png.size();
        item.contentType = 1;
        Write(transaction.Directory_nid_no_patch() / "content.png", Png.data(), Png.size());
        return item;
    };
    second.Commit_nid_no_patch(entry(second));
    const auto middle = Catalog::Read_nid_no_patch();
    Require(middle.entries.size() == before.entries.size() + 1, "pending export became visible");
    first.Commit_nid_no_patch(entry(first));
    const auto after = Catalog::Read_nid_no_patch();
    Require(after.entries.size() == before.entries.size() + 2, "transaction publication lost entry");
    Require(after.updateId > middle.updateId, "out of order publication did not advance update id");
    for (const auto& directory : std::filesystem::directory_iterator(Catalog::Root_nid_no_patch()))
        Require(directory.path().filename().string().find(".pending-") != 0, "failed export left pending files");
}

void CheckCatalogChild() {
    namespace Catalog = ContentCatalog_nid_no_patch;
    const auto before = Catalog::Read_nid_no_patch();
    Require(!before.entries.empty(), "child process cannot read persistent catalog");
    const auto& first = before.entries.front();
    Require(Read(first.path.c_str()) == std::vector<std::uint8_t>(Png.begin(), Png.end()),
        "child process read different content bytes");
    Catalog::Transaction_nid_no_patch transaction;
    Catalog::Entry item;
    item.id = transaction.Id_nid_no_patch();
    item.title = "separate process";
    item.mimeType = "image/png";
    item.path = transaction.GuestPath_nid_no_patch("content.png");
    item.size = Png.size();
    item.contentType = 1;
    Write(transaction.Directory_nid_no_patch() / "content.png", Png.data(), Png.size());
    transaction.Commit_nid_no_patch(item);
    const auto after = Catalog::Read_nid_no_patch();
    Require(after.updateId > before.updateId && after.entries.size() == before.entries.size() + 1,
        "child process publication lost generation");
}

void CheckCatalogProcess(const std::filesystem::path& executable) {
    const auto before = ContentCatalog_nid_no_patch::Read_nid_no_patch();
#ifdef _WIN32
    const auto root = std::filesystem::current_path().wstring();
    const auto program = executable.wstring();
    const auto status = _wspawnl(_P_WAIT, program.c_str(), program.c_str(), L"--catalog-child", root.c_str(), static_cast<wchar_t*>(nullptr));
    Require(status == 0, "catalog child process failed");
#else
    const auto root = std::filesystem::current_path().string();
    const auto process = fork();
    Require(process >= 0, "catalog child fork failed");
    if (process == 0) {
        execl(executable.c_str(), executable.c_str(), "--catalog-child", root.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    Require(waitpid(process, &status, 0) == process && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "catalog child process failed");
#endif
    const auto after = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(after.entries.size() == before.entries.size() + 1 && after.updateId > before.updateId,
        "parent process missed child publication");
}

}

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--catalog-child") == 0) {
        std::filesystem::current_path(argv[2]);
        CheckCatalogChild();
        return 0;
    }
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto previous = std::filesystem::current_path();
    const auto root = previous / ("export-" + std::to_string(ContentCatalog_nid_no_patch::CurrentTick_nid_no_patch()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    Write("source.png", Png.data(), Png.size());
    Write("thumbnail.png", Png.data(), Png.size());
    Write("invalid", "invalid media", 13);
    Allocator allocator;
    ContentExportInitParam2 init{
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), &allocator, 256, 0, 0};
    Require(sceContentExportStart() == NoInit, "start before init");
    Require(sceContentExportFinish(1) == NoInit, "finish before init");
    Require(Throws([] { sceContentExportInit2(nullptr); }), "null init accepted");
    Require(Throws([] { sceContentExportTerm(); }), "term before init");
    auto invalidInit = init;
    invalidInit.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&invalidInit); }), "missing allocator accepted");
    invalidInit = init;
    invalidInit.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&invalidInit); }), "nonzero init reserved accepted");
    invalidInit = init;
    invalidInit.buffer_size = 255;
    Require(sceContentExportInit2(&invalidInit) == InvalidParam, "undersized buffer accepted");
    Require(sceContentExportInit2(&init) == 0, "init failed");
    Require(Throws([&] { sceContentExportInit2(&init); }), "double init accepted");
    std::array<int, 10> sessions;
    for (auto& session : sessions) { session = sceContentExportStart(); Require(session > 0, "session creation failed"); }
    Require(sceContentExportStart() == ExecutionMax, "session limit ignored");
    Require(sceContentExportTerm() == Busy, "term discarded sessions");
    for (const auto session : sessions) Require(sceContentExportFinish(session) == 0, "session finish failed");
    Require(Throws([&] { sceContentExportFinish(sessions[0]); }), "finished session accepted");
    const auto session = sceContentExportStart();
    auto param = MakeParam();
    std::array<char, 1025> path{};
    Require(sceContentExportFromFile(session, &param, "/source.png", path.data(), path.size()) == 0, "file export failed");
    Require(Read(path.data()) == std::vector<std::uint8_t>(Png.begin(), Png.end()), "file bytes changed");
    const auto filePath = std::string(path.data());
    auto snapshot = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(snapshot.entries.size() == 1 && snapshot.entries[0].title == param.title, "file metadata missing");
    Require(snapshot.entries[0].width == 2 && snapshot.entries[0].height == 1, "image dimensions wrong");
    Require(snapshot.entries[0].accounts[0] == USER_SERVICE_INITIAL_USER_ID, "owner missing");
    Require(sceContentExportFromFileWithThumbnail(session, &param, "/source.png", "/thumbnail.png", path.data(), path.size()) == 0,
        "thumbnail export failed");
    snapshot = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(snapshot.entries.size() == 2 && !snapshot.entries.back().iconPath.empty(), "thumbnail path missing");
    Require(Read(snapshot.entries.back().iconPath.c_str()) == std::vector<std::uint8_t>(Png.begin(), Png.end()), "thumbnail bytes changed");
    for (const bool separateDone : {false, true}) {
        Chunks chunks{0, 0, true, session, separateDone};
        Require(sceContentExportFromData(session, &param, Png.size(), Provide, &chunks, path.data(), path.size()) == 0, "callback export failed");
        Require(Read(path.data()) == std::vector<std::uint8_t>(Png.begin(), Png.end()), "callback bytes changed");
    }
    const auto beforeFailure = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    std::memset(path.data(), 'x', path.size());
    const auto untouched = path;
    Require(sceContentExportFromFile(session, nullptr, "/source.png", path.data(), path.size()) == InvalidParam, "null parameter accepted");
    Require(sceContentExportFromFile(session, &param, "/source.png", path.data(), 2) == InvalidParam, "short output accepted");
    Require(sceContentExportFromFile(session, &param, "/missing", path.data(), path.size()) == FileNotFound, "missing file accepted");
    Require(sceContentExportFromFile(session, &param, "/invalid", path.data(), path.size()) == UnsupportedFormat, "invalid media accepted");
    Require(sceContentExportFromFileWithThumbnail(session, &param, "/source.png", "/invalid", path.data(), path.size()) == UnsupportedThumbnail,
        "invalid thumbnail accepted");
    auto unsupported = MakeParam("unsupported", "application/octet-stream");
    Require(sceContentExportFromFile(session, &unsupported, "/source.png", path.data(), path.size()) == UnsupportedFormat, "unknown format accepted");
    unsupported = param;
    unsupported.reserved[0] = 1;
    Require(Throws([&] { sceContentExportFromFile(session, &unsupported, "/source.png", path.data(), path.size()); }), "unknown field accepted");
    unsupported = param;
    std::memset(unsupported.title, 't', sizeof(unsupported.title));
    Require(sceContentExportFromFile(session, &unsupported, "/source.png", path.data(), path.size()) == InvalidParam, "unterminated title accepted");
    for (int failure = 1; failure <= 5; ++failure) {
        Chunks chunks{0, failure};
        Require(sceContentExportFromData(session, &param, Png.size(), Provide, &chunks, path.data(), path.size()) == DataProvideError,
            "bad callback accepted");
    }
    allocator.fail = true;
    Require(sceContentExportFromFile(session, &param, "/source.png", path.data(), path.size()) == NoMemory, "allocator failure ignored");
    allocator.fail = false;
    Require(path == untouched, "failed operation modified output");
    const auto afterFailure = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(afterFailure.entries.size() == beforeFailure.entries.size() && afterFailure.updateId == beforeFailure.updateId, "failed operation published content");
    const std::array<std::uint8_t, 12> pixels{255,0,0,0,255,0,0,0,255,255,255,255};
    const auto jpeg = Decoder::Jpeg::Encode(pixels, 2, 2, 3, 90);
    Write("source.jpg", jpeg.data(), jpeg.size());
    auto jpegParam = MakeParam("jpeg", "image/jpeg");
    Require(sceContentExportFromFile(session, &jpegParam, "/source.jpg", path.data(), path.size()) == 0, "JPEG export failed");
    Require(Read(path.data()) == jpeg, "JPEG bytes changed");
    constexpr std::array<std::uint8_t, 35> gif{
        'G','I','F','8','9','a',1,0,1,0,0x80,0,0,0,0,0,255,255,255,
        0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,0x01,0,0x3b};
    Write("source.gif", gif.data(), gif.size());
    auto gifParam = MakeParam("gif", "image/gif");
    Require(sceContentExportFromFile(session, &gifParam, "/source.gif", path.data(), path.size()) == 0, "GIF export failed");
    Require(Read(path.data()) == std::vector<std::uint8_t>(gif.begin(), gif.end()), "GIF bytes changed");
    auto videoParam = MakeParam("video", "video/mp4");
    Require(Throws([&] { sceContentExportFromFile(session, &videoParam, "/source.png", path.data(), path.size()); }), "unsupported video succeeded");
    Require(sceContentExportFromFile(session, &jpegParam, "/source.png", path.data(), path.size()) == UnsupportedFormat, "mismatched MIME accepted");
    std::array<int, 2> parallel{session, sceContentExportStart()};
    std::array<std::thread, 2> threads;
    std::atomic<bool> reading{true};
    std::thread reader([&] {
        std::int64_t generation = 0;
        while (reading) {
            const auto snapshot = ContentCatalog_nid_no_patch::Read_nid_no_patch();
            Require(snapshot.updateId >= generation, "snapshot generation went backwards");
            generation = snapshot.updateId;
            for (const auto& item : snapshot.entries) {
                Require(item.status == 1 && std::filesystem::exists(ResolvePath_nid_no_patch(item.path.c_str())),
                    "snapshot exposed incomplete publication");
            }
        }
    });
    for (std::size_t index = 0; index < threads.size(); ++index) {
        threads[index] = std::thread([&, index] {
            std::array<char, 1025> output{};
            Require(sceContentExportFromFile(parallel[index], &param, "/source.png", output.data(), output.size()) == 0, "parallel export failed");
        });
    }
    for (auto& thread : threads) thread.join();
    reading = false;
    reader.join();
    for (const auto id : parallel) Require(sceContentExportFinish(id) == 0, "parallel session finish failed");
    CheckCatalogTransactions();
    CheckCatalogProcess(executable);
    Require(allocator.allocations == allocator.releases, "allocator ownership leaked");
    const auto persisted = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(sceContentExportTerm() == 0, "term failed");
    Require(sceContentExportInit2(&init) == 0, "reinit failed");
    const auto reopened = ContentCatalog_nid_no_patch::Read_nid_no_patch();
    Require(reopened.entries.size() == persisted.entries.size() && reopened.updateId == persisted.updateId, "catalog did not persist");
    Require(Read(filePath.c_str()) == std::vector<std::uint8_t>(Png.begin(), Png.end()), "persisted file changed");
    Require(sceContentExportTerm() == 0, "final term failed");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}

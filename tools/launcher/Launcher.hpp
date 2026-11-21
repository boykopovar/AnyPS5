#pragma once

#include <windows.h>
#include <filesystem>
#include <map>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Launcher {
namespace fs = std::filesystem;
struct Game {
    std::wstring id, title, executable, workingDirectory;
    std::wstring titleId, status = L"Unverified", gpu, driver, fullscreen;
    std::vector<std::wstring> arguments;
    std::map<std::wstring, std::optional<std::wstring>> environment;
};
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
std::wstring Quote(const std::wstring& text);
fs::path Resolve(const fs::path& root, const std::wstring& value);
std::wstring Store(const fs::path& root, const fs::path& path);
fs::path ExecutablePath();
fs::path DataRoot(const fs::path& executable);
void Validate(const Game& game);
Game DescribeLauncherGame(const fs::path& root, const fs::path& directory);
Game MakeGame(const fs::path& root, const fs::path& executable);
struct Discovery {
    fs::path path;
    bool prepared;
    std::wstring reason;
};
fs::path NormalizeLibraryPath(std::wstring value);
std::vector<Discovery> ScanLibraryFolder(const fs::path& folder, const std::function<bool(size_t, const fs::path&)>& progress);
std::vector<fs::path> DiscoverLauncherGames(const fs::path& folder);
struct LibraryFolder {
    std::wstring id, path;
};
std::vector<LibraryFolder> ReadLibraryFolders(const fs::path& root);
void SaveLibraryFolders(const fs::path& root, const std::vector<LibraryFolder>& folders);
bool AddLibraryFolder(std::vector<LibraryFolder>& folders, const fs::path& root, const fs::path& path);
std::vector<std::wstring> RescanLibrary(const fs::path& root, std::vector<Game>& games, const std::vector<LibraryFolder>& folders);
fs::path PickLibraryFolder(HWND owner = nullptr);
std::vector<Game> ReadLauncherLibrary(const fs::path& root);
void SaveLauncherLibrary(const fs::path& root, const std::vector<Game>& games);
void ExportLauncherCollection(const fs::path& root, const fs::path& destination);
bool AddGame(std::vector<Game>& games, const fs::path& root, const Game& game);
std::wstring RuntimeNotes(const fs::path& root, const Game& game);
struct Command {
    fs::path application, workingDirectory;
    std::wstring commandLine;
    std::vector<wchar_t> environment;
};
Command BuildLauncherCommand(const fs::path& root, const Game& game, const fs::path& log);
class Session {
public:
    Session(const fs::path& root, const Game& game);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    std::optional<DWORD> Poll();
    const fs::path& Folder() const;
private:
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    fs::path folder;
    std::optional<DWORD> exitCode;
};
class InstanceLock {
public:
    explicit InstanceLock(const fs::path& root);
    ~InstanceLock();
private:
    HANDLE handle = INVALID_HANDLE_VALUE;
};
}

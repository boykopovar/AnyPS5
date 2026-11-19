#include "Launcher.hpp"
#include <shellapi.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Launcher;
static int checks = 0;
static void Require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
template<class TFunction> static void Reject(TFunction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "Expected operation to fail");
}
static std::string Read(const fs::path& path) { std::ifstream input(path, std::ios::binary); return {(std::istreambuf_iterator<char>(input)), {}}; }
static DWORD Wait(Session& session) {
    for (int i = 0; i < 1000; ++i) { auto code = session.Poll(); if (code) return *code; Sleep(10); }
    throw std::runtime_error("Fixture process timed out");
}
int main() {
    int argc;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1 && std::wstring(argv[1]) == L"--fixture") {
        if (argc != 6 || std::wstring(argv[2]) != L"space & %PATH% !x!" || std::wstring(argv[3]) != L"quote\"end\\" || std::wstring(argv[4]) != L"" || std::wstring(argv[5]) != L"caf\u00e9") return 40;
        std::ofstream("fixture-save.txt") << "persistent fixture save";
        std::cout << "direct fixture reached\n";
        return 42;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--sleep") { Sleep(800); return 0; }
    if (argc > 1 && std::wstring(argv[1]) == L"--detached") {
        auto executable = ExecutablePath(); auto command = Quote(executable.wstring()) + L" --sleep";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION child{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) return 1;
        CloseHandle(child.hThread); CloseHandle(child.hProcess); return 42;
    }
    wchar_t temporary[MAX_PATH]; GetTempPathW(MAX_PATH, temporary);
    auto root = fs::path(temporary) / (L"AnyPS5 launcher tests space caf\u00e9-" + std::to_wstring(GetCurrentProcessId()));
    try {
        fs::create_directory(root);
        auto folder = root / L"Games/game & percent% bang!";
        fs::create_directories(folder / "app0/sce_sys"); fs::create_directory(folder / "libs");
        fs::copy_file(ExecutablePath(), folder / "app.exe");
        std::ofstream(folder / "app0/sce_sys/param.json") << R"({"localizedParameters":{"en-US":{"titleName":"Snow \u2603 \ud83c\udf00"}},"titleId":"TEST00001"})";
        auto game = MakeGame(root, folder / "app.exe");
        Require(game.title == L"Snow \u2603 \U0001f300" && game.titleId == L"TEST00001", "Metadata Unicode escapes");
        Require(!fs::path(game.executable).is_absolute(), "Collection-relative launch path");
        game.gpu = L"fixture GPU"; game.environment[L"FIXTURE_TOKEN"] = L"child only";
        std::vector<Game> games; Require(AddGame(games, root, game), "Add entry");
        Require(!AddGame(games, root, MakeGame(root, folder / "app.exe")), "Duplicate entry");
        SaveLauncherLibrary(root, games); games[0].title = L"Saved settings"; SaveLauncherLibrary(root, games);
        Require(fs::is_regular_file(root / "data/library.txt.bak"), "Previous library backup");
        Require(ReadLauncherLibrary(root)[0].gpu == L"fixture GPU", "Settings round trip");
        auto damaged = root / "damaged"; fs::create_directories(damaged / "data"); std::ofstream(damaged / "data/library.txt") << "corrupt";
        Reject([&] { ReadLauncherLibrary(damaged); }); Require(Read(damaged / "data/library.txt") == "corrupt", "Corrupt data remains intact");
        InstanceLock lock(root); Reject([&] { InstanceLock second(root); });
        auto wrapper = folder / L"Launch %PATH% & !x!.cmd";
        std::ofstream(wrapper) << "@echo off\necho wrapper reached\necho %FIXTURE_TOKEN%\ncd\nexit /b 42\n";
        auto batch = MakeGame(root, wrapper); batch.environment[L"FIXTURE_TOKEN"] = L"child only";
        auto command = BuildLauncherCommand(root, batch, root / "runtime.log");
        Require(command.commandLine.find(folder.wstring()) == std::wstring::npos, "Batch path must not become command syntax");
        Session batchSession(root, batch); Require(Wait(batchSession) == 42, "Real Windows batch exit");
        Require(Read(batchSession.Folder() / "output.log").find("wrapper reached") != std::string::npos, "Windows special-character batch path");
        Require(Read(batchSession.Folder() / "output.log").find("child only") != std::string::npos, "Child environment");
        Require(GetEnvironmentVariableW(L"FIXTURE_TOKEN", nullptr, 0) == 0, "Parent environment unchanged");
        batch.arguments = {L"bad argument"}; Reject([&] { BuildLauncherCommand(root, batch, root / "runtime.log"); });
        auto direct = MakeGame(root, ExecutablePath()); direct.workingDirectory = Store(root, folder);
        direct.arguments = {L"--fixture", L"space & %PATH% !x!", L"quote\"end\\", L"", L"caf\u00e9"};
        Session directSession(root, direct); Require(Wait(directSession) == 42, "Real direct process argument quoting");
        Require(Read(folder / "fixture-save.txt") == "persistent fixture save", "Process working directory and save write");
        std::ofstream(folder / "Launch.cmd") << "@echo off\nexit /b 0\n";
        auto scan = DiscoverLauncherGames(root / "Games"); Require(scan.size() == 1 && scan[0].filename() == L"Launch.cmd", "Shallow scan prefers existing launcher");
        std::vector<LibraryFolder> roots;
        Require(AddLibraryFolder(roots, root, root / "Games"), "Add library root");
        Require(!AddLibraryFolder(roots, root, root / "Games" / "."), "Duplicate root preserves settings");
        auto inputs = root / "Unconverted input"; fs::create_directories(inputs);
        std::ofstream(inputs / "eboot.bin", std::ios::binary) << "\x7f" "ELF input fixture";
        Require(AddLibraryFolder(roots, root, inputs), "Multiple library roots");
        Require(DescribeLauncherGame(root, inputs).title == L"Unconverted input", "Preparation preview describes a folder without inventing an executable");
        SaveLibraryFolders(root, roots);
        Require(ReadLibraryFolders(root).size() == 2, "Library roots round trip");
        auto scanned = games;
        auto scanNotes = RescanLibrary(root, scanned, roots);
        Require(scanned.size() == 1 && scanned[0].gpu == L"fixture GPU", "Rescan preserves existing settings without duplicate game folders");
        Require(std::any_of(scanNotes.begin(), scanNotes.end(), [](const std::wstring& note) { return note.find(L"Needs preparation") != std::wstring::npos; }), "Original input is never registered as executable");
        fs::remove(inputs / "eboot.bin"); fs::remove(inputs);
        scanNotes = RescanLibrary(root, scanned, roots);
        Require(std::any_of(scanNotes.begin(), scanNotes.end(), [](const std::wstring& note) { return note.find(L"Missing library folder") != std::wstring::npos; }), "Missing root gives actionable message");
        auto nested = folder / "app0/nested"; fs::create_directories(nested / "app0"); fs::create_directory(nested / "libs"); std::ofstream(nested / "app.exe") << "fixture";
        Require(DiscoverLauncherGames(root / "Games").size() == 1, "Scan never walks game assets");
        auto detached = MakeGame(root, ExecutablePath()); detached.arguments = {L"--detached"};
        Session detachedSession(root, detached); Sleep(150);
        Require(!detachedSession.Poll(), "Detached child keeps the game active after wrapper exit");
        Require(Wait(detachedSession) == 42, "Process tree completion retains wrapper exit status");
        Require(NormalizeLibraryPath(L" \t\"" + folder.wstring() + L"\"\r\n") == fs::canonical(folder), "Explorer quoted path with Unicode and punctuation");
        Require(NormalizeLibraryPath(folder.wstring()) == fs::canonical(folder), "Plain Windows folder path");
        Reject([&] { NormalizeLibraryPath(L"relative/path"); });
        Reject([&] { NormalizeLibraryPath(folder.wstring() + L"\ncmd.exe"); });
        size_t visited = 0;
        auto cancelled = ScanLibraryFolder(root / "Games", [&](size_t count, const fs::path&) { visited = count; return false; });
        Require(visited == 1 && cancelled.empty(), "Cancellation stops discovery before enumeration");
        auto deeper = root / "Nested/level1/level2/Title";
        fs::create_directories(deeper / "app0"); fs::create_directory(deeper / "libs"); fs::copy_file(ExecutablePath(), deeper / "app.exe");
        auto discovered = ScanLibraryFolder(root / "Nested", [](size_t, const fs::path&) { return true; });
        Require(discovered.size() == 1 && discovered[0].prepared, "Bounded nested game discovery");
        std::ofstream(root / "Nested/random.exe") << "MZ";
        auto original = root / "Nested/Original"; fs::create_directory(original); std::ofstream(original / "eboot.bin") << "SELF fixture";
        discovered = ScanLibraryFolder(root / "Nested", [](size_t, const fs::path&) { return true; });
        Require(discovered.size() == 2 && std::count_if(discovered.begin(), discovered.end(), [](const auto& item) { return !item.prepared; }) == 1, "Original inputs need preparation; arbitrary executable ignored");
        auto package = root / "Package";
        fs::create_directories(package / "TITLE/sce_sys");
        std::ofstream(package / "helper.elf") << "ELF helper fixture";
        std::ofstream(package / "TITLE/eboot.bin") << "SELF main fixture";
        std::ofstream(package / "TITLE/helper.elf") << "ELF companion fixture";
        discovered = ScanLibraryFolder(package, [](size_t, const fs::path&) { return true; });
        Require(discovered.size() == 1 && discovered[0].path == package / "TITLE", "Package helpers do not hide the nested game");
        auto destination = root / "Relocated collection with spaces";
        ExportLauncherCollection(root, destination);
        auto moved = ReadLauncherLibrary(destination);
        Require(moved.size() == 1 && !fs::path(moved[0].executable).is_absolute(), "Export registers relative paths");
        Require(Read(Resolve(destination, moved[0].workingDirectory) / "fixture-save.txt") == "persistent fixture save", "Export preserves save data");
        Require(Read(Resolve(root, games[0].workingDirectory) / "fixture-save.txt") == "persistent fixture save", "Export preserves original data");
        Reject([&] { ExportLauncherCollection(root, destination); });
        games.clear(); SaveLauncherLibrary(root, games);
        Require(fs::is_regular_file(folder / "fixture-save.txt"), "Removing entries preserves saves");
        std::cout << checks << " launcher checks passed; Windows direct/batch processes executed, no Windows tests skipped\n";
    } catch (const std::exception& error) { std::cerr << error.what() << "\nFixtures retained at " << Utf8(root.wstring()) << '\n'; return 1; }
    return 0;
}

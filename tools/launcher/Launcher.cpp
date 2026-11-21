/* SPDX-License-Identifier: MIT
MIT License

Copyright (c) 2026 Chris

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "Launcher.hpp"
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <shlobj.h>
#include <stdexcept>

namespace Launcher {
static void Check(bool success, const char* operation) {
    if (!success) throw std::runtime_error(std::string(operation) + ": Windows error " + std::to_string(GetLastError()));
}

std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    Check(size != 0, "Decode UTF-8");
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    Check(size != 0, "Encode UTF-8");
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring Lower(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t value) { return std::towlower(value); });
    return text;
}

static std::wstring UniqueId() {
    GUID guid;
    Check(SUCCEEDED(CoCreateGuid(&guid)), "Create identifier");
    wchar_t text[40];
    StringFromGUID2(guid, text, 40);
    return text;
}

std::wstring Quote(const std::wstring& text) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : text) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += ch;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

fs::path Resolve(const fs::path& root, const std::wstring& value) {
    return fs::absolute(root / fs::path(value)).lexically_normal();
}

std::wstring Store(const fs::path& root, const fs::path& path) {
    auto absolute = fs::absolute(path).lexically_normal();
    auto relative = absolute.lexically_relative(fs::absolute(root).lexically_normal());
    if (!relative.empty() && *relative.begin() != L"..") return relative.generic_wstring();
    return absolute.wstring();
}

fs::path ExecutablePath() {
    std::wstring buffer(32768, 0);
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    Check(size > 0 && size < buffer.size(), "Find executable");
    buffer.resize(size);
    return buffer;
}

fs::path DataRoot(const fs::path& executable) {
    if (fs::exists(executable.parent_path() / "portable.flag")) return executable.parent_path();
    return fs::path(ANYPS5_LAUNCHER_DATA_ROOT);
}

void Validate(const Game& game) {
    auto validText = [](const std::wstring& text) { return text.find(L'\0') == std::wstring::npos; };
    for (const auto* text : {&game.id, &game.title, &game.executable, &game.workingDirectory, &game.titleId, &game.status, &game.gpu, &game.driver, &game.fullscreen})
        if (!validText(*text)) throw std::runtime_error("Settings contain a NUL character");
    GUID guid;
    if (FAILED(CLSIDFromString(game.id.c_str(), &guid))) throw std::runtime_error("Invalid game identifier");
    if (game.title.empty() || game.executable.empty() || game.workingDirectory.empty()) throw std::runtime_error("Name, launch file and working directory are required");
    auto extension = Lower(fs::path(game.executable).extension().wstring());
    if (extension != L".exe" && extension != L".cmd" && extension != L".bat") throw std::runtime_error("Select a converted .exe or existing .cmd/.bat, not eboot.bin");
    if (extension != L".exe" && !game.arguments.empty()) throw std::runtime_error("Batch arguments are unsupported; retain arguments in the existing script");
    if (game.status != L"Unverified" && game.status != L"Working" && game.status != L"Needs attention") throw std::runtime_error("Unknown test result");
    if (!game.fullscreen.empty() && game.fullscreen != L"0" && game.fullscreen != L"1") throw std::runtime_error("Window mode must be blank, 0 or 1");
    for (const auto& arg : game.arguments) if (!validText(arg)) throw std::runtime_error("Invalid argument");
    std::set<std::wstring> keys;
    for (const auto& [key, value] : game.environment) {
        if (key.empty() || key.find(L'=') != std::wstring::npos || !validText(key) || (value && !validText(*value)) || !keys.insert(Lower(key)).second)
            throw std::runtime_error("Invalid or duplicate environment key");
    }
}

static std::wstring JsonString(const std::string& text, size_t& at) {
    if (text.at(at++) != '"') throw std::runtime_error("Expected JSON string");
    std::string bytes;
    while (at < text.size()) {
        unsigned char ch = text[at++];
        if (ch == '"') return Wide(bytes);
        if (ch < 32) throw std::runtime_error("Invalid JSON string");
        if (ch != '\\') { bytes += static_cast<char>(ch); continue; }
        char escape = text.at(at++);
        if (escape == 'u') {
            auto unit = [&]() {
                unsigned value = 0;
                for (int i = 0; i < 4; ++i) {
                    char digit = text.at(at++);
                    if (!std::isxdigit(static_cast<unsigned char>(digit))) throw std::runtime_error("Invalid JSON escape");
                    value = value * 16 + (digit <= '9' ? digit - '0' : std::tolower(digit) - 'a' + 10);
                }
                return static_cast<wchar_t>(value);
            };
            std::wstring wide(1, unit());
            if (wide[0] >= 0xd800 && wide[0] <= 0xdbff) {
                if (text.substr(at, 2) != "\\u") throw std::runtime_error("Invalid JSON surrogate");
                at += 2;
                wide += unit();
            }
            bytes += Utf8(wide);
        } else {
            const std::string escapes = "\"\\/bfnrt";
            const std::string values = "\"\\/\b\f\n\r\t";
            auto index = escapes.find(escape);
            if (index == std::string::npos) throw std::runtime_error("Invalid JSON escape");
            bytes += values[index];
        }
    }
    throw std::runtime_error("Unterminated JSON string");
}

static std::wstring MetadataValue(const fs::path& path, const std::wstring& key) {
    if (!fs::is_regular_file(path) || fs::file_size(path) > 2 * 1024 * 1024) return {};
    std::ifstream input(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(input)), {});
    for (size_t at = 0; at < text.size();) {
        if (text[at] != '"') { ++at; continue; }
        auto value = JsonString(text, at);
        auto next = text.find_first_not_of(" \r\n\t", at);
        if (value != key || next == std::string::npos || text[next] != ':') continue;
        at = text.find_first_not_of(" \r\n\t", next + 1);
        if (at != std::string::npos && text[at] == '"') return JsonString(text, at);
        if (at == std::string::npos) break;
    }
    return {};
}

Game DescribeLauncherGame(const fs::path& root, const fs::path& directory) {
    Game game;
    game.id = UniqueId();
    game.workingDirectory = Store(root, directory);
    game.title = directory.filename().wstring();
    try {
        auto metadata = directory / "app0/sce_sys/param.json";
        if (!fs::exists(metadata)) metadata = directory / "sce_sys/param.json";
        auto title = MetadataValue(metadata, L"titleName");
        if (!title.empty()) game.title = title;
        game.titleId = MetadataValue(metadata, L"titleId");
    } catch (const std::exception&) {}
    return game;
}

Game MakeGame(const fs::path& root, const fs::path& executable) {
    auto game = DescribeLauncherGame(root, executable.parent_path());
    game.executable = Store(root, executable);
    Validate(game);
    return game;
}

fs::path NormalizeLibraryPath(std::wstring value) {
    auto begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) throw std::runtime_error("Paste an absolute Windows folder path");
    value = value.substr(begin, value.find_last_not_of(L" \t\r\n") - begin + 1);
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') value = value.substr(1, value.size() - 2);
    if (value.size() < 3 || !iswalpha(value[0]) || value[1] != L':' || (value[2] != L'\\' && value[2] != L'/') || value.find_first_of(L"\r\n\t\"<>|?*") != std::wstring::npos || value.find(L':', 2) != std::wstring::npos || value.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Use a local absolute drive path, such as D:\\Games. UNC and device paths are not supported; map network storage to a drive first.");
    fs::path path(value);
    if (!fs::is_directory(path)) throw std::runtime_error("Folder is missing or inaccessible. Check the path, drive connection and Windows permissions.");
    return fs::canonical(path);
}

static bool DirectoryLink(const fs::path& path) {
    auto attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

static bool WindowsExecutable(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    char magic[2]{}; input.read(magic, 2);
    if (magic[0] != 'M' || magic[1] != 'Z') return false;
    input.seekg(60); unsigned offset = 0; input.read(reinterpret_cast<char*>(&offset), 4);
    if (!input || offset > 16 * 1024 * 1024) return false;
    input.seekg(offset); char signature[4]{}; input.read(signature, 4);
    unsigned short machine = 0; input.read(reinterpret_cast<char*>(&machine), 2);
    return input && machine == 0x8664 && std::string(signature, 4) == std::string("PE\0\0", 4);
}

std::vector<Discovery> ScanLibraryFolder(const fs::path& folder, const std::function<bool(size_t, const fs::path&)>& progress) {
    std::vector<std::pair<fs::path, unsigned>> pending{{folder, 0}};
    std::vector<Discovery> found;
    size_t visited = 0;
    while (!pending.empty()) {
        auto [directory, depth] = pending.back(); pending.pop_back();
        if (Lower(directory.filename().wstring()).starts_with(L".anyps5-preparing-")) continue;
        if (!progress(++visited, directory)) return {};
        if (visited > 20000) throw std::runtime_error("Scan reached 20,000 folders. Choose a more specific game folder.");
        std::map<std::wstring, fs::path> files;
        std::vector<fs::path> children;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (DirectoryLink(entry.path())) continue;
            if (entry.is_regular_file()) files[Lower(entry.path().filename().wstring())] = entry.path();
            else if (entry.is_directory()) children.push_back(entry.path());
            if (files.size() + children.size() > 20000) throw std::runtime_error("Folder contains too many entries. Choose a more specific folder.");
        }
        bool layout = fs::is_directory(directory / "app0") && fs::is_directory(directory / "libs");
        std::vector<fs::path> executables;
        if (layout) for (const auto& [name, path] : files)
            if (Lower(path.extension().wstring()) == L".exe" && name != L"relinker.exe" && name != L"uninstall.exe" && WindowsExecutable(path)) executables.push_back(path);
        if (layout && !executables.empty()) {
            fs::path executable;
            if (files.contains(L"launch.cmd")) executable = files[L"launch.cmd"];
            else if (files.contains(L"app.exe") && WindowsExecutable(files[L"app.exe"])) executable = files[L"app.exe"];
            else if (files.contains(L"eboot.exe") && WindowsExecutable(files[L"eboot.exe"])) executable = files[L"eboot.exe"];
            else if (executables.size() == 1) executable = executables.front();
            if (!executable.empty()) { found.push_back({executable, true, L"Ready to launch; gameplay unverified"}); continue; }
        }
        bool input = files.contains(L"eboot.bin") || files.contains(L"app.elf") || files.contains(L"eboot.elf");
        bool nestedGame = std::any_of(children.begin(), children.end(), [](const fs::path& child) {
            for (const auto& directory : {child, child / "app0"})
                for (const auto* name : {"eboot.bin", "eboot.elf", "app.elf"}) if (fs::is_regular_file(directory / name)) return true;
            return false;
        });
        if (!nestedGame) for (const auto& [name, path] : files) if (Lower(path.extension().wstring()) == L".pkg" || Lower(path.extension().wstring()) == L".self" || Lower(path.extension().wstring()) == L".elf") input = true;
        if (input || layout) {
            found.push_back({directory, false, L"Needs Windows preparation. Choose Prepare & add for extracted ELF or decrypted SELF inputs; package extraction and decryption are unsupported."});
            continue;
        }
        if (depth >= 5) continue;
        if (pending.size() + children.size() > 20000) throw std::runtime_error("Scan has too many pending folders. Choose a more specific root.");
        for (const auto& child : children) {
            auto name = Lower(child.filename().wstring());
            if (name.starts_with(L".anyps5-preparing-") || name == L"app0" || name == L"libs" || name == L".git" || name == L"node_modules" || name == L"cache" || name == L"caches" || name == L"shadercache" || name == L"build" || name == L"saves" || name == L"logs" || name == L".local" || name == L"3rdparty") continue;
            pending.emplace_back(child, depth + 1);
        }
    }
    return found;
}

std::vector<fs::path> DiscoverLauncherGames(const fs::path& folder) {
    std::vector<fs::path> found;
    for (const auto& item : ScanLibraryFolder(folder, [](size_t, const fs::path&) { return true; })) if (item.prepared) found.push_back(item.path);
    return found;
}

static void WriteText(std::ostream& output, const std::wstring& value) { output << std::quoted(Utf8(value)) << '\n'; }
static std::wstring ReadText(std::istream& input) {
    std::string value;
    if (!(input >> std::quoted(value))) throw std::runtime_error("Corrupt library; original left untouched");
    return Wide(value);
}
static size_t ReadCount(std::istream& input) {
    size_t count;
    if (!(input >> count) || count > 4096) throw std::runtime_error("Invalid library count");
    return count;
}

std::vector<LibraryFolder> ReadLibraryFolders(const fs::path& root) {
    auto file = root / "data/library-folders.txt";
    if (!fs::exists(file)) return {};
    if (fs::file_size(file) > 1024 * 1024) throw std::runtime_error("Library folders file exceeds 1 MiB");
    std::ifstream input(file, std::ios::binary);
    if (ReadText(input) != L"AnyPS5 Library Folders 1") throw std::runtime_error("Unsupported library folders format");
    std::vector<LibraryFolder> folders;
    std::set<std::wstring> ids;
    auto count = ReadCount(input);
    for (size_t i = 0; i < count; ++i) {
        LibraryFolder folder{ReadText(input), ReadText(input)};
        GUID guid;
        if (FAILED(CLSIDFromString(folder.id.c_str(), &guid)) || folder.path.empty() || folder.path.find(L'\0') != std::wstring::npos || !ids.insert(folder.id).second) throw std::runtime_error("Invalid library folder");
        folders.push_back(folder);
    }
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Unexpected library folders data");
    return folders;
}

void SaveLibraryFolders(const fs::path& root, const std::vector<LibraryFolder>& folders) {
    if (folders.size() > 4096) throw std::runtime_error("Too many library folders");
    fs::create_directories(root / "data");
    auto file = root / "data/library-folders.txt";
    auto temp = root / "data" / (UniqueId() + L".tmp");
    try {
        std::ofstream output(temp, std::ios::binary);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        WriteText(output, L"AnyPS5 Library Folders 1"); output << folders.size() << '\n';
        std::set<std::wstring> ids;
        for (const auto& folder : folders) {
            GUID guid;
            if (FAILED(CLSIDFromString(folder.id.c_str(), &guid)) || folder.path.empty() || folder.path.find(L'\0') != std::wstring::npos || !ids.insert(folder.id).second) throw std::runtime_error("Invalid library folder");
            WriteText(output, folder.id); WriteText(output, folder.path);
        }
        output.close();
        if (fs::exists(file)) Check(ReplaceFileW(file.c_str(), temp.c_str(), (root / "data/library-folders.txt.bak").c_str(), 0, nullptr, nullptr), "Save library folders");
        else Check(MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_WRITE_THROUGH), "Save library folders");
    } catch (...) { fs::remove(temp); throw; }
}

bool AddLibraryFolder(std::vector<LibraryFolder>& folders, const fs::path& root, const fs::path& path) {
    auto absolute = fs::canonical(path);
    if (!fs::is_directory(absolute)) throw std::runtime_error("Selected library folder is missing");
    for (const auto& folder : folders) {
        auto existing = Resolve(root, folder.path);
        if (_wcsicmp(existing.c_str(), absolute.c_str()) == 0 || (fs::exists(existing) && fs::equivalent(existing, absolute))) return false;
    }
    folders.push_back({UniqueId(), Store(root, absolute)});
    return true;
}

std::vector<std::wstring> RescanLibrary(const fs::path& root, std::vector<Game>& games, const std::vector<LibraryFolder>& folders) {
    std::vector<std::wstring> notes;
    for (const auto& folder : folders) {
        auto path = Resolve(root, folder.path);
        try {
            if (!fs::is_directory(path)) { notes.push_back(L"Missing library folder: " + path.wstring() + L". Reconnect its drive or update Library folders."); continue; }
            auto prepared = DiscoverLauncherGames(path);
            size_t added = 0;
            for (const auto& executable : prepared) {
                bool registered = std::any_of(games.begin(), games.end(), [&](const Game& game) {
                    auto existing = Resolve(root, game.workingDirectory);
                    return _wcsicmp(existing.c_str(), executable.parent_path().c_str()) == 0 || (fs::is_directory(existing) && fs::equivalent(existing, executable.parent_path()));
                });
                if (!registered) added += AddGame(games, root, MakeGame(root, executable));
            }
            notes.push_back(path.wstring() + L": " + std::to_wstring(added) + L" added; " + std::to_wstring(prepared.size() - added) + L" already registered (settings preserved).");
            std::vector<fs::path> candidates{path};
            for (const auto& entry : fs::directory_iterator(path)) if (entry.is_directory() && !entry.is_symlink()) candidates.push_back(entry.path());
            for (const auto& candidate : candidates) {
                if (!fs::is_regular_file(candidate / "eboot.bin")) continue;
                if (std::any_of(prepared.begin(), prepared.end(), [&](const fs::path& file) { return file.parent_path() == candidate; })) continue;
                char magic[4]{}; std::ifstream input(candidate / "eboot.bin", std::ios::binary); input.read(magic, 4);
                std::wstring type = std::string(magic, 4) == "\x7f" "ELF" ? L"ELF input" : (std::string(magic, 4) == std::string("\x4f\x15\x3d\x1d", 4) || std::string(magic, 4) == std::string("\x54\x14\xf5\xee", 4) ? L"SELF input" : L"unconverted or invalid input");
                notes.push_back(L"Needs preparation: " + candidate.wstring() + L" (" + type + L"). Unwrap decrypted SELF and convert for Windows; eboot.bin will not be launched.");
            }
        } catch (const std::exception& error) { notes.push_back(path.wstring() + L": " + Wide(error.what())); }
    }
    return notes;
}

fs::path PickLibraryFolder(HWND owner) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) throw std::runtime_error("Could not open the Windows folder picker");
    dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
    fs::path path;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR name = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) { path = name; CoTaskMemFree(name); }
            item->Release();
        }
    }
    dialog->Release(); return path;
}

std::vector<Game> ReadLauncherLibrary(const fs::path& root) {
    auto path = root / "data/library.txt";
    if (!fs::exists(path)) return {};
    if (fs::file_size(path) > 8 * 1024 * 1024) throw std::runtime_error("Library exceeds 8 MiB");
    std::ifstream input(path, std::ios::binary);
    if (ReadText(input) != L"AnyPS5 Launcher 1") throw std::runtime_error("Unsupported library format; original left untouched");
    auto count = ReadCount(input);
    std::vector<Game> games;
    std::set<std::wstring> ids;
    for (size_t i = 0; i < count; ++i) {
        Game game;
        for (auto* text : {&game.id, &game.title, &game.executable, &game.workingDirectory, &game.titleId, &game.status, &game.gpu, &game.driver, &game.fullscreen}) *text = ReadText(input);
        auto arguments = ReadCount(input);
        for (size_t j = 0; j < arguments; ++j) game.arguments.push_back(ReadText(input));
        auto environment = ReadCount(input);
        for (size_t j = 0; j < environment; ++j) {
            auto key = ReadText(input);
            auto mode = ReadText(input);
            if (mode != L"set" && mode != L"unset") throw std::runtime_error("Invalid environment mode");
            std::optional<std::wstring> value;
            if (mode == L"set") value = ReadText(input);
            if (!game.environment.emplace(key, value).second) throw std::runtime_error("Duplicate environment key");
        }
        Validate(game);
        if (!ids.insert(game.id).second) throw std::runtime_error("Duplicate game identifier");
        games.push_back(game);
    }
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Unexpected library data; original left untouched");
    return games;
}

void SaveLauncherLibrary(const fs::path& root, const std::vector<Game>& games) {
    if (games.size() > 4096) throw std::runtime_error("Too many library entries");
    std::set<std::wstring> ids;
    for (const auto& game : games) { Validate(game); if (!ids.insert(game.id).second) throw std::runtime_error("Duplicate identifier"); }
    fs::create_directories(root / "data");
    auto path = root / "data/library.txt";
    auto temporary = root / "data" / (L"library-" + UniqueId() + L".tmp");
    try {
        std::ofstream output(temporary, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        WriteText(output, L"AnyPS5 Launcher 1"); output << games.size() << '\n';
        for (const auto& game : games) {
            for (auto* text : {&game.id, &game.title, &game.executable, &game.workingDirectory, &game.titleId, &game.status, &game.gpu, &game.driver, &game.fullscreen}) WriteText(output, *text);
            output << game.arguments.size() << '\n';
            for (const auto& arg : game.arguments) WriteText(output, arg);
            output << game.environment.size() << '\n';
            for (const auto& [key, value] : game.environment) { WriteText(output, key); WriteText(output, value ? L"set" : L"unset"); if (value) WriteText(output, *value); }
        }
        output.close();
        if (fs::exists(path)) Check(ReplaceFileW(path.c_str(), temporary.c_str(), (root / "data/library.txt.bak").c_str(), 0, nullptr, nullptr), "Replace library");
        else Check(MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH), "Save library");
    } catch (...) { fs::remove(temporary); throw; }
}

bool AddGame(std::vector<Game>& games, const fs::path& root, const Game& game) {
    Validate(game);
    for (const auto& existing : games) {
        auto left = Resolve(root, existing.executable), right = Resolve(root, game.executable);
        if (Lower(left.wstring()) == Lower(right.wstring()) || (fs::exists(left) && fs::exists(right) && fs::equivalent(left, right))) return false;
    }
    games.push_back(game);
    return true;
}

static void Expand(std::wstring& value, const std::wstring& name, const std::wstring& replacement);

void ExportLauncherCollection(const fs::path& root, const fs::path& destination) {
    if (fs::exists(destination)) throw std::runtime_error("Export destination already exists; choose a fresh folder");
    auto games = ReadLauncherLibrary(root);
    std::set<std::wstring> names;
    for (const auto& game : games) {
        auto source = Resolve(root, game.workingDirectory);
        auto relative = Resolve(root, game.executable).lexically_relative(source);
        if (relative.empty() || *relative.begin() == L"..") throw std::runtime_error("Portable export requires the launch file inside its working folder");
        auto nested = fs::absolute(destination).lexically_normal().lexically_relative(source);
        if (!nested.empty() && *nested.begin() != L"..") throw std::runtime_error("Export destination cannot be inside a game folder");
        if (!game.driver.empty()) {
            auto driver = Resolve(root, game.driver).lexically_relative(source);
            if (driver.empty() || *driver.begin() == L"..") throw std::runtime_error("Portable export requires the complete Vulkan driver installation inside the game folder");
        }
        for (const auto& file : fs::recursive_directory_iterator(source)) if (file.is_symlink()) throw std::runtime_error("Portable export refuses linked files; copy resources into the game folder first");
    }
    fs::create_directories(destination / "Games");
    for (auto& game : games) {
        auto source = Resolve(root, game.workingDirectory);
        auto relative = Resolve(root, game.executable).lexically_relative(source);
        auto name = game.titleId;
        if (name.empty() || name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::wstring::npos || !names.insert(name).second) name = game.id;
        auto target = destination / "Games" / name;
        auto space = fs::space(destination);
        uintmax_t needed = 0;
        for (const auto& file : fs::recursive_directory_iterator(source)) if (file.is_regular_file()) needed += file.file_size();
        if (needed > space.available) throw std::runtime_error("Insufficient space for complete game export");
        fs::copy(source, target, fs::copy_options::recursive);
        for (auto& [key, value] : game.environment) if (value) Expand(*value, source.wstring(), L"{game}");
        if (!game.driver.empty()) game.driver = Store(destination, target / Resolve(root, game.driver).lexically_relative(source));
        game.executable = Store(destination, target / relative);
        game.workingDirectory = Store(destination, target);
    }
    SaveLauncherLibrary(destination, games);
    std::vector<LibraryFolder> folders;
    AddLibraryFolder(folders, destination, destination / "Games");
    SaveLibraryFolders(destination, folders);
    std::ofstream(destination / "portable.flag") << "AnyPS5 Launcher portable collection\n";
}

std::wstring RuntimeNotes(const fs::path& root, const Game& game) {
    auto folder = Resolve(root, game.executable).parent_path();
    size_t count = 0;
    if (fs::is_directory(folder / "libs")) for (const auto& file : fs::directory_iterator(folder / "libs")) if (file.path().extension() == L".prx") ++count;
    std::wstring notes = std::to_wstring(count) + L" PRX files. MinGW DLLs:";
    for (auto name : {L"libgcc_s_seh-1.dll", L"libstdc++-6.dll", L"libwinpthread-1.dll"}) if (!fs::is_regular_file(folder / "libs" / name)) notes += L" missing " + std::wstring(name);
    if (notes.ends_with(L"DLLs:")) notes += L" all three present.";
    return notes + L"\r\nFile presence does not confirm compatibility. " + (fs::path(game.executable).is_absolute() ? L"External folder." : L"Collection-relative folder.");
}

struct EnvironmentOrder {
    bool operator()(const std::wstring& left, const std::wstring& right) const { return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN; }
};
static void Expand(std::wstring& value, const std::wstring& name, const std::wstring& replacement) {
    for (size_t at = 0; (at = value.find(name, at)) != std::wstring::npos; at += replacement.size()) value.replace(at, name.size(), replacement);
}

Command BuildLauncherCommand(const fs::path& root, const Game& game, const fs::path& log) {
    Validate(game);
    Command result;
    result.application = Resolve(root, game.executable);
    result.workingDirectory = Resolve(root, game.workingDirectory);
    if (!fs::is_regular_file(result.application) || !fs::is_directory(result.workingDirectory)) throw std::runtime_error("Launch file or working directory is missing");
    std::map<std::wstring, std::wstring, EnvironmentOrder> environment;
    auto block = GetEnvironmentStringsW();
    Check(block != nullptr, "Read environment");
    for (auto* current = block; *current; current += wcslen(current) + 1) {
        std::wstring entry(current);
        auto separator = entry.find(L'=', entry[0] == L'=' ? 1 : 0);
        if (separator != std::wstring::npos) environment[entry.substr(0, separator)] = entry.substr(separator + 1);
    }
    FreeEnvironmentStringsW(block);
    for (const auto& [key, value] : game.environment) {
        if (!value) { environment.erase(key); continue; }
        auto expanded = *value;
        Expand(expanded, L"{launcher}", root.wstring()); Expand(expanded, L"{game}", result.workingDirectory.wstring());
        environment[key] = expanded;
    }
    if (!game.gpu.empty()) environment[L"ANYPS5_GPU"] = game.gpu;
    if (!game.driver.empty()) {
        auto driver = Resolve(root, game.driver);
        if (!fs::is_regular_file(driver)) throw std::runtime_error("Selected Vulkan driver JSON is missing");
        environment[L"VK_DRIVER_FILES"] = driver.wstring();
    }
    if (!game.fullscreen.empty()) environment[L"ANYPS5_FULLSCREEN"] = game.fullscreen;
    if (!environment.contains(L"ANYPS5_LOG_FILE")) environment[L"ANYPS5_LOG_FILE"] = log.wstring();
    if (Lower(result.application.extension().wstring()) == L".exe") {
        result.commandLine = Quote(result.application.wstring());
        for (const auto& argument : game.arguments) result.commandLine += L" " + Quote(argument);
    } else {
        environment[L"ANYPS5_LAUNCH_TARGET"] = result.application.wstring();
        std::wstring system(32768, 0);
        auto size = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
        Check(size > 0 && size < system.size(), "Find command processor"); system.resize(size);
        result.application = fs::path(system) / "cmd.exe";
        result.commandLine = Quote(result.application.wstring()) + L" /d /v:off /s /c \"\"%ANYPS5_LAUNCH_TARGET%\"\"";
    }
    for (const auto& [key, value] : environment) {
        auto text = key + L"=" + value;
        result.environment.insert(result.environment.end(), text.begin(), text.end()); result.environment.push_back(0);
    }
    result.environment.push_back(0);
    return result;
}

Session::Session(const fs::path& root, const Game& game) {
    folder = root / "logs" / game.id / UniqueId();
    fs::create_directories(folder);
    auto command = BuildLauncherCommand(root, game, folder / "runtime.log");
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE output = CreateFileW((folder / "output.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(output != INVALID_HANDLE_VALUE, "Open session log");
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input; startup.hStdOutput = output; startup.hStdError = output;
    PROCESS_INFORMATION info{};
    BOOL success = CreateProcessW(command.application.c_str(), command.commandLine.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | CREATE_SUSPENDED, command.environment.data(), command.workingDirectory.c_str(), &startup, &info);
    DWORD error = GetLastError(); CloseHandle(output); if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (!success) { std::ofstream(folder / "launch-error.txt") << "Windows error " << error; SetLastError(error); Check(false, "Launch game"); }
    job = CreateJobObjectW(nullptr, nullptr);
    if (!job || !AssignProcessToJobObject(job, info.hProcess)) {
        auto error = GetLastError(); TerminateProcess(info.hProcess, 1); CloseHandle(info.hThread); CloseHandle(info.hProcess);
        if (job) { CloseHandle(job); job = nullptr; }
        SetLastError(error); Check(false, "Track launch process tree");
    }
    process = info.hProcess;
    auto resumed = ResumeThread(info.hThread); CloseHandle(info.hThread);
    if (resumed == static_cast<DWORD>(-1)) {
        auto error = GetLastError(); TerminateProcess(process, 1); CloseHandle(process); process = nullptr;
        CloseHandle(job); job = nullptr; SetLastError(error); Check(false, "Start tracked game");
    }
    std::ofstream record(folder / "session.txt");
    record << "Process created: " << info.dwProcessId << "\nTarget: " << Utf8(Resolve(root, game.executable).wstring()) << "\nWorking directory: " << Utf8(command.workingDirectory.wstring()) << "\nGameplay: unverified\n";
}

Session::~Session() {
    if (job) CloseHandle(job);
    if (process) {
        if (!exitCode) std::ofstream(folder / "session.txt", std::ios::app) << "Monitoring ended; running game is not terminated\n";
        CloseHandle(process);
    }
}
std::optional<DWORD> Session::Poll() {
    if (exitCode) return exitCode;
    if (job) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        Check(QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr), "Read game process tree");
        if (accounting.ActiveProcesses) return {};
    }
    if (WaitForSingleObject(process, 0) != WAIT_OBJECT_0) return {};
    DWORD code;
    Check(GetExitCodeProcess(process, &code), "Read exit status");
    exitCode = code;
    std::ofstream(folder / "session.txt", std::ios::app) << "Launch process exited: " << code << "\n";
    return exitCode;
}
const fs::path& Session::Folder() const { return folder; }

InstanceLock::InstanceLock(const fs::path& root) {
    fs::create_directories(root / "data");
    handle = CreateFileW((root / "data/launcher.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(handle != INVALID_HANDLE_VALUE, "Collection is locked or not writable");
}
InstanceLock::~InstanceLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
}

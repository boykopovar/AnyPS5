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
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

using namespace Launcher;

static fs::path collection;
static std::vector<Game> games;
static std::vector<LibraryFolder> libraryFolders;
static std::vector<size_t> visible;
static std::map<std::wstring, std::unique_ptr<Session>> sessions;
static HWND mainWindow, gameList, searchBox, details;
static HFONT font;
static const wchar_t* help =
    L"GETTING STARTED\r\n\r\n"
    L"1. Add game: choose an already prepared Launch.cmd, app.exe or eboot.exe. Original eboot.bin is conversion input, not a Windows program.\r\n\r\n"
    L"2. Check Settings. Retain the working directory, arguments and environment from the successful run. Existing scripts keep their own settings and redirects.\r\n\r\n"
    L"3. Play starts a process. Open Logs for output and exit status. Mark Working only after observing gameplay, input, audio and saves yourself.\r\n\r\n"
    L"Library folders saves multiple roots. Add folder opens the Windows picker; Rescan checks each root and its immediate children for app0, libs and an unambiguous Windows launch file. Existing settings are preserved. Original eboot.bin inputs are labelled Needs preparation and never launched.\r\n\r\n"
    L"GPU, driver JSON and window mode overrides affect only the child process. Blank fields inherit the environment. Window mode: blank = inherit, 0 = windowed, 1 = fullscreen.\r\n\r\n"
    L"Arguments: one per line, .exe targets only. Environment: KEY=value sets a variable; -KEY removes it. {game} and {launcher} expand to the working and collection folders. Scripts may override these values.\r\n\r\n"
    L"Logs are stored per game and launch under the collection's logs folder. Scripts may also write game-local logs. A detached script's exit is not the game's exit. Closing the launcher leaves games running.\r\n\r\n"
    L"Normal builds keep data outside the build tree. Portable packages contain portable.flag: keep the entire folder, including Games, data, logs and Tools. Relative entries move with it; External folder entries do not.\r\n\r\n"
    L"Keep complete game folders and saves together. Remove entry only removes a library record. Packaging creates a fresh folder; it never replaces your collection. Close games before copying saves. CPU, GPU and Vulkan driver requirements still apply.\r\n";

static void Error(HWND owner, const std::exception& error) { MessageBoxW(owner, Wide(error.what()).c_str(), L"AnyPS5 Launcher", MB_ICONERROR); }
static HWND Control(HWND owner, const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int width, int height, int id) {
    HWND window = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!window) throw std::runtime_error("Could not create launcher control");
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return window;
}
static std::wstring Text(HWND control) {
    std::wstring value(GetWindowTextLengthW(control) + 1, 0);
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}
static std::vector<std::wstring> Lines(const std::wstring& text) {
    std::vector<std::wstring> result;
    std::wistringstream input(text);
    std::wstring line;
    while (std::getline(input, line)) { if (line.ends_with(L'\r')) line.pop_back(); result.push_back(line); }
    return result;
}
static void OpenFolder(const fs::path& path) {
    if (!fs::is_directory(path)) throw std::runtime_error("Folder does not exist yet");
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(mainWindow, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) throw std::runtime_error("Could not open folder");
}
static fs::path PickFile(HWND owner) {
    std::wstring path(32768, 0);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"Prepared launch files\0*.exe;*.cmd;*.bat\0All files\0*.*\0";
    dialog.lpstrFile = path.data(); dialog.nMaxFile = static_cast<DWORD>(path.size()); dialog.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return {};
    path.resize(wcslen(path.c_str())); return path;
}
static fs::path PickFolder(HWND owner) {
    return PickLibraryFolder(owner);
}
static Game* Selected() {
    auto index = SendMessageW(gameList, LB_GETCURSEL, 0, 0);
    return index == LB_ERR || static_cast<size_t>(index) >= visible.size() || visible[index] >= games.size() ? nullptr : &games[visible[index]];
}
static void ShowDetails() {
    auto* game = Selected();
    if (!game) { SetWindowTextW(details, L"Add a prepared game or scan its parent folder. Gameplay remains unverified until tested."); return; }
    auto value = game->title + L"  " + game->titleId + L"\r\n" + Resolve(collection, game->executable).wstring() + L"\r\nWorking directory: " + Resolve(collection, game->workingDirectory).wstring() + L"\r\n" + RuntimeNotes(collection, *game);
    if (sessions.contains(game->id)) value += L"\r\nLaunch process running. Gameplay is not automatically verified.";
    SetWindowTextW(details, value.c_str());
}
static void Refresh() {
    auto* selected = Selected(); auto id = selected ? selected->id : L"";
    auto query = Text(searchBox);
    auto lower = [](std::wstring value) { std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return std::towlower(c); }); return value; };
    query = lower(query);
    SendMessageW(gameList, LB_RESETCONTENT, 0, 0); visible.clear();
    for (size_t i = 0; i < games.size(); ++i) {
        const auto& game = games[i];
        if (lower(game.title + L" " + game.titleId).find(query) == std::wstring::npos) continue;
        visible.push_back(i);
        auto label = game.title + L"   |   " + game.titleId + L"   |   " + game.status;
        label += sessions.contains(game.id) ? L"   |   Running" : L"";
        SendMessageW(gameList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (game.id == id) SendMessageW(gameList, LB_SETCURSEL, visible.size() - 1, 0);
    }
    if (SendMessageW(gameList, LB_GETCURSEL, 0, 0) == LB_ERR && !visible.empty()) SendMessageW(gameList, LB_SETCURSEL, 0, 0);
    ShowDetails();
}
struct Editor {
    Game game;
    HWND fields[10]{};
};
static void RefreshFolders(HWND list) {
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& folder : libraryFolders) {
        auto path = Resolve(collection, folder.path);
        auto label = (fs::is_directory(path) ? L"" : L"MISSING: ") + path.wstring();
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}
static LRESULT CALLBACK FoldersProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    try {
        if (message == WM_CREATE) {
            Control(window, L"STATIC", L"Saved library roots. Removing a root does not remove registered games or saves.", 0, 15, 15, 735, 32, 0);
            auto list = Control(window, L"LISTBOX", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 15, 55, 735, 230, 200);
            RefreshFolders(list);
            Control(window, L"BUTTON", L"Add folder...", WS_TABSTOP, 15, 300, 150, 35, 1101);
            Control(window, L"BUTTON", L"Remove root", WS_TABSTOP, 180, 300, 150, 35, 1102);
            Control(window, L"BUTTON", L"Rescan all", WS_TABSTOP, 345, 300, 150, 35, 1103);
            Control(window, L"BUTTON", L"Close", WS_TABSTOP, 600, 300, 150, 35, IDCANCEL);
        } else if (message == WM_COMMAND) {
            auto id = LOWORD(wParam);
            auto list = GetDlgItem(window, 200);
            if (id == IDCANCEL) DestroyWindow(window);
            else if (id == 1101) {
                auto path = PickFolder(window);
                if (!path.empty()) {
                    auto updated = libraryFolders;
                    if (!AddLibraryFolder(updated, collection, path)) MessageBoxW(window, L"That library folder is already saved.", L"Library folders", MB_OK);
                    else { SaveLibraryFolders(collection, updated); libraryFolders = std::move(updated); RefreshFolders(list); }
                }
            } else if (id == 1102) {
                auto index = SendMessageW(list, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && static_cast<size_t>(index) < libraryFolders.size()) {
                    auto updated = libraryFolders; updated.erase(updated.begin() + index);
                    SaveLibraryFolders(collection, updated); libraryFolders = std::move(updated); RefreshFolders(list);
                }
            } else if (id == 1103) {
                auto updated = games;
                auto notes = RescanLibrary(collection, updated, libraryFolders);
                SaveLauncherLibrary(collection, updated); games = std::move(updated); Refresh(); RefreshFolders(list);
                std::wstring summary;
                for (const auto& note : notes) summary += note + L"\r\n\r\n";
                MessageBoxW(window, summary.empty() ? L"Add a library folder before rescanning." : summary.c_str(), L"Rescan", MB_OK);
            }
        } else if (message == WM_DESTROY) { EnableWindow(mainWindow, TRUE); SetForegroundWindow(mainWindow); }
    } catch (const std::exception& error) { Error(window, error); }
    return DefWindowProcW(window, message, wParam, lParam);
}
static LRESULT CALLBACK EditorProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* editor = reinterpret_cast<Editor*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    try {
        if (message == WM_CREATE) {
            editor = static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor));
            const wchar_t* labels[] = {L"Game name", L"Launch file", L"Working folder", L"Test result", L"GPU name", L"Vulkan driver JSON", L"Window mode (blank/0/1)", L"Arguments (one per line)", L"Environment (KEY=value / -KEY)", L"Title ID"};
            std::wstring args, env;
            for (const auto& arg : editor->game.arguments) args += arg + L"\r\n";
            for (const auto& [key, value] : editor->game.environment) env += (value ? key + L"=" + *value : L"-" + key) + L"\r\n";
            std::wstring values[] = {editor->game.title, editor->game.executable, editor->game.workingDirectory, editor->game.status, editor->game.gpu, editor->game.driver, editor->game.fullscreen, args, env, editor->game.titleId};
            int y = 20;
            for (int i = 0; i < 10; ++i) {
                int height = i == 7 || i == 8 ? 70 : 26;
                Control(window, L"STATIC", labels[i], 0, 18, y + 4, 235, 24, 0);
                editor->fields[i] = Control(window, L"EDIT", values[i].c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | (height > 26 ? ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL : 0), 255, y, 480, height, 100 + i);
                y += height + 12;
            }
            Control(window, L"STATIC", L"Results: Unverified / Working / Needs attention. Paths may be collection-relative.\r\nBlank graphics settings inherit the environment. Existing scripts retain their settings.", 0, 18, y, 720, 50, 0);
            Control(window, L"BUTTON", L"Save game", WS_TABSTOP | BS_DEFPUSHBUTTON, 500, y + 58, 110, 32, IDOK);
            Control(window, L"BUTTON", L"Cancel", WS_TABSTOP, 625, y + 58, 110, 32, IDCANCEL);
        } else if (message == WM_COMMAND && LOWORD(wParam) == IDOK) {
            auto game = editor->game;
            game.title = Text(editor->fields[0]); game.executable = Store(collection, Resolve(collection, Text(editor->fields[1])));
            game.workingDirectory = Store(collection, Resolve(collection, Text(editor->fields[2]))); game.status = Text(editor->fields[3]);
            game.gpu = Text(editor->fields[4]); game.driver = Text(editor->fields[5]); game.fullscreen = Text(editor->fields[6]);
            if (!game.driver.empty()) game.driver = Store(collection, Resolve(collection, game.driver));
            game.arguments = Lines(Text(editor->fields[7])); game.environment.clear(); game.titleId = Text(editor->fields[9]);
            for (const auto& line : Lines(Text(editor->fields[8]))) {
                if (line.empty()) continue;
                auto equal = line.find(L'=');
                if (equal != std::wstring::npos) { if (!game.environment.emplace(line.substr(0, equal), line.substr(equal + 1)).second) throw std::runtime_error("Duplicate environment key"); }
                else if (line[0] == L'-') { if (!game.environment.emplace(line.substr(1), std::nullopt).second) throw std::runtime_error("Duplicate environment key"); }
                else throw std::runtime_error("Environment lines must be KEY=value or -KEY");
            }
            Validate(game);
            if (!fs::is_regular_file(Resolve(collection, game.executable)) || !fs::is_directory(Resolve(collection, game.workingDirectory))) throw std::runtime_error("Launch file or working folder is missing");
            auto updated = games;
            for (auto& existing : updated) {
                if (existing.id != game.id && _wcsicmp(Resolve(collection, existing.executable).c_str(), Resolve(collection, game.executable).c_str()) == 0) throw std::runtime_error("Launch file already registered");
                if (existing.id == game.id) existing = game;
            }
            SaveLauncherLibrary(collection, updated); games = std::move(updated); Refresh(); DestroyWindow(window);
        } else if (message == WM_COMMAND && LOWORD(wParam) == IDCANCEL) DestroyWindow(window);
        else if (message == WM_DESTROY) { delete editor; EnableWindow(mainWindow, TRUE); SetForegroundWindow(mainWindow); }
    } catch (const std::exception& error) { Error(window, error); }
    return DefWindowProcW(window, message, wParam, lParam);
}

static LRESULT CALLBACK LauncherWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    try {
        if (message == WM_CREATE) {
            mainWindow = window;
            Control(window, L"STATIC", L"AnyPS5 Launcher", 0, 22, 15, 400, 30, 0);
            Control(window, L"BUTTON", L"Add game", WS_TABSTOP, 22, 55, 130, 34, 1001);
            Control(window, L"BUTTON", L"Library folders", WS_TABSTOP, 165, 55, 130, 34, 1002);
            Control(window, L"BUTTON", L"Getting started", WS_TABSTOP, 310, 55, 150, 34, 1003);
            Control(window, L"STATIC", L"Search", 0, 485, 62, 60, 28, 0);
            searchBox = Control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 545, 57, 430, 30, 20);
            gameList = Control(window, L"LISTBOX", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 22, 105, 953, 290, 21);
            details = Control(window, L"EDIT", L"", ES_READONLY | ES_MULTILINE | WS_VSCROLL, 22, 410, 953, 135, 22);
            Control(window, L"BUTTON", L"Play", WS_TABSTOP, 22, 560, 130, 38, 1004);
            Control(window, L"BUTTON", L"Settings", WS_TABSTOP, 165, 560, 130, 38, 1005);
            Control(window, L"BUTTON", L"Game folder", WS_TABSTOP, 310, 560, 130, 38, 1006);
            Control(window, L"BUTTON", L"Logs", WS_TABSTOP, 455, 560, 130, 38, 1007);
            Control(window, L"BUTTON", L"Remove entry", WS_TABSTOP, 600, 560, 130, 38, 1008);
            Control(window, L"STATIC", (L"Collection: " + collection.wstring()).c_str(), 0, 22, 615, 953, 35, 0);
            SetTimer(window, 1, 400, nullptr); Refresh();
        } else if (message == WM_COMMAND) {
            int action = LOWORD(wParam);
            if (action >= 1001 && action <= 1008) action -= 1000;
            else if (action != 20 && action != 21) return DefWindowProcW(window, message, wParam, lParam);
            if (action == 20 && HIWORD(wParam) == EN_CHANGE) Refresh();
            else if (action == 21 && HIWORD(wParam) == LBN_SELCHANGE) ShowDetails();
            else if (action == 2) {
                HWND dialog = CreateWindowExW(WS_EX_CONTROLPARENT, L"AnyPS5LibraryFolders", L"Library folders", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 790, 400, window, nullptr, GetModuleHandleW(nullptr), nullptr);
                if (dialog) EnableWindow(window, FALSE);
            }
            else if (action == 1) {
                auto path = PickFile(window);
                if (!path.empty()) {
                    auto updated = games;
                    auto files = action == 1 ? std::vector<fs::path>{path} : DiscoverLauncherGames(path);
                    size_t added = 0;
                    for (const auto& file : files) added += AddGame(updated, collection, MakeGame(collection, file));
                    SaveLauncherLibrary(collection, updated); games = std::move(updated); Refresh();
                    MessageBoxW(window, (std::to_wstring(added) + L" games added. Ambiguous folders are skipped; use Add game for those.").c_str(), L"Library", MB_OK);
                }
            } else if (action == 3) {
                HWND helpWindow = CreateWindowExW(0, L"STATIC", L"Getting started - AnyPS5 Launcher", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 850, 660, window, nullptr, GetModuleHandleW(nullptr), nullptr);
                Control(helpWindow, L"EDIT", help, ES_READONLY | ES_MULTILINE | WS_VSCROLL, 0, 0, 830, 610, 0);
            } else if (auto* game = Selected()) {
                if (action == 4) {
                    if (sessions.contains(game->id)) throw std::runtime_error("This launch process is already running");
                    sessions[game->id] = std::make_unique<Session>(collection, *game); Refresh();
                } else if (action == 5) {
                    if (sessions.contains(game->id)) throw std::runtime_error("Wait for this launch process to exit before editing settings");
                    auto editor = std::make_unique<Editor>(); editor->game = *game;
                    HWND dialog = CreateWindowExW(WS_EX_CONTROLPARENT, L"AnyPS5GameEditor", L"Game settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 780, 650, window, nullptr, GetModuleHandleW(nullptr), editor.get());
                    if (dialog) { editor.release(); EnableWindow(window, FALSE); }
                } else if (action == 6) OpenFolder(Resolve(collection, game->workingDirectory));
                else if (action == 7) { fs::create_directories(collection / "logs" / game->id); OpenFolder(collection / "logs" / game->id); }
                else if (action == 8) {
                    if (sessions.contains(game->id)) throw std::runtime_error("Wait for this launch process to exit before removing the entry");
                    auto updated = games; auto id = game->id;
                    std::erase_if(updated, [&](const Game& item) { return item.id == id; });
                    SaveLauncherLibrary(collection, updated); games = std::move(updated); Refresh();
                }
            }
        } else if (message == WM_TIMER) {
            bool changed = false;
            for (auto it = sessions.begin(); it != sessions.end();) {
                auto code = it->second->Poll();
                if (code) {
                    SetWindowTextW(details, (L"Launch process exited with code " + std::to_wstring(*code) + L". See Logs. Exit status does not verify gameplay.").c_str());
                    it = sessions.erase(it); changed = true;
                } else ++it;
            }
            if (changed) Refresh();
        } else if (message == WM_DESTROY) { sessions.clear(); PostQuitMessage(0); return 0; }
    } catch (const std::exception& error) { Error(window, error); }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool commandMode = false;
    try {
        collection = DataRoot(ExecutablePath());
        int count = 0;
        auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
        std::vector<std::wstring> args(raw + 1, raw + count); LocalFree(raw);
        commandMode = !args.empty();
        if (args.size() >= 2 && args[0] == L"--data-root") { collection = fs::absolute(args[1]); args.erase(args.begin(), args.begin() + 2); }
        InstanceLock lock(collection); games = ReadLauncherLibrary(collection); libraryFolders = ReadLibraryFolders(collection);
        if (!args.empty()) {
            if (args.size() == 2 && args[0] == L"--register") {
                AddLibraryFolder(libraryFolders, collection, args[1]);
                for (const auto& file : DiscoverLauncherGames(args[1])) AddGame(games, collection, MakeGame(collection, file));
                SaveLibraryFolders(collection, libraryFolders); SaveLauncherLibrary(collection, games); return 0;
            }
            if (args.size() == 2 && args[0] == L"--run") {
                for (const auto& game : games) if (game.titleId == args[1] || game.id == args[1]) {
                    Session session(collection, game);
                    while (!session.Poll()) Sleep(100);
                    return static_cast<int>(*session.Poll());
                }
                throw std::runtime_error("Requested game is not registered");
            }
            if (args.size() == 2 && args[0] == L"--export") { ExportLauncherCollection(collection, fs::absolute(args[1])); return 0; }
            throw std::runtime_error("Usage: AnyPS5Launcher [--data-root folder] [--register prepared-parent | --run title-id]");
        }
        SetProcessDPIAware();
        font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        WNDCLASSW cls{}; cls.hInstance = instance; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        cls.lpfnWndProc = EditorProcedure; cls.lpszClassName = L"AnyPS5GameEditor"; RegisterClassW(&cls);
        cls.lpfnWndProc = FoldersProcedure; cls.lpszClassName = L"AnyPS5LibraryFolders"; RegisterClassW(&cls);
        cls.lpfnWndProc = LauncherWindowProcedure; cls.lpszClassName = L"AnyPS5Launcher"; RegisterClassW(&cls);
        HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"AnyPS5 Launcher", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 1020, 710, nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("Could not create launcher window");
        ShowWindow(window, show);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            HWND active = GetActiveWindow();
            if (active == window && !IsWindowEnabled(window)) active = GetLastActivePopup(window);
            if (!IsDialogMessageW(active, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        CoUninitialize(); return 0;
    } catch (const std::exception& error) {
        if (commandMode) std::cerr << "AnyPS5 Launcher: " << error.what() << '\n';
        else Error(nullptr, error);
        CoUninitialize(); return 1;
    }
}

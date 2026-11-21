#include "Launcher.hpp"
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <atomic>
#include <mutex>
#include <thread>
#include <set>

using namespace Launcher;

static std::string ReadFileText(const fs::path& path) {
    auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot read helper file: Windows error " + std::to_string(GetLastError()));
    std::unique_ptr<void, decltype(&CloseHandle)> handle(file, &CloseHandle);
    std::string result;
    for (;;) {
        char buffer[4096]; DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) throw std::runtime_error("Cannot read helper file: Windows error " + std::to_string(GetLastError()));
        if (!read) return result;
        result.append(buffer, read);
        if (result.size() > 4 * 1024 * 1024) throw std::runtime_error("Helper file exceeds its size limit");
    }
}
static std::string Json(const std::wstring& text) {
    auto utf8 = Utf8(text);
    std::ostringstream result; result << '"';
    for (unsigned char ch : utf8) {
        if (ch == '"' || ch == '\\') result << '\\' << ch;
        else if (ch < 32) result << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
        else result << ch;
    }
    result << '"'; return result.str();
}
struct InternetHandle {
    HINTERNET value = nullptr;
    explicit InternetHandle(HINTERNET input) : value(input) { if (!value) throw std::runtime_error("Windows HTTP operation failed: " + std::to_string(GetLastError())); }
    ~InternetHandle() { WinHttpCloseHandle(value); }
    InternetHandle(const InternetHandle&) = delete;
};
struct Configuration {
    fs::path collection, state, token, engine;
    std::wstring host;
    INTERNET_PORT port = 0;
};
static Configuration ReadConfiguration(const fs::path& file) {
    std::istringstream input(ReadFileText(file));
    std::vector<std::wstring> lines; std::string line;
    while (std::getline(input, line)) { if (line.ends_with('\r')) line.pop_back(); lines.push_back(Wide(line)); }
    if ((lines.size() != 5 && lines.size() != 6) || lines[0] != L"AnyPS5 Desktop Helper 1") throw std::runtime_error("Invalid desktop helper configuration");
    Configuration config; config.collection = lines[1]; config.state = lines[2]; config.token = lines[3];
    if (lines.size() == 6) { config.engine = lines[5]; if (!config.engine.is_absolute()) throw std::runtime_error("Engine directory must be absolute"); }
    if (!config.collection.is_absolute() || !config.state.is_absolute() || !config.token.is_absolute()) throw std::runtime_error("Helper storage paths must be absolute");
    URL_COMPONENTSW parts{}; parts.dwStructSize = sizeof(parts); parts.dwHostNameLength = static_cast<DWORD>(-1); parts.dwUrlPathLength = static_cast<DWORD>(-1); parts.dwExtraInfoLength = static_cast<DWORD>(-1); parts.dwUserNameLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(lines[4].c_str(), 0, 0, &parts)) throw std::runtime_error("Invalid backend URL");
    config.host.assign(parts.lpszHostName, parts.dwHostNameLength); config.port = parts.nPort;
    if (parts.nScheme != INTERNET_SCHEME_HTTP || (config.host != L"127.0.0.1" && config.host != L"localhost") || parts.dwExtraInfoLength || parts.dwUserNameLength || (parts.dwUrlPathLength && std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) != L"/")) throw std::runtime_error("Backend must be a loopback HTTP origin without credentials or a path");
    return config;
}
static void Record(const Configuration& config, const std::string& message) {
    fs::create_directories(config.state);
    SYSTEMTIME now; GetSystemTime(&now);
    std::ofstream log(config.state / "helper.log", std::ios::app);
    log << now.wYear << '-' << now.wMonth << '-' << now.wDay << ' ' << now.wHour << ':' << now.wMinute << ':' << now.wSecond << " UTC " << message << '\n';
}
static std::string Request(const Configuration& config, const wchar_t* method, const wchar_t* path, const std::string& body = {}) {
    auto token = ReadFileText(config.token);
    while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) token.pop_back();
    if (token.size() < 32 || token.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string::npos) throw std::runtime_error("Invalid pairing token file");
    InternetHandle session(WinHttpOpen(L"AnyPS5DesktopHelper/1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    WinHttpSetTimeouts(session.value, 3000, 3000, 4000, 4000);
    InternetHandle connection(WinHttpConnect(session.value, config.host.c_str(), config.port, 0));
    InternetHandle request(WinHttpOpenRequest(connection.value, method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    auto headers = L"Authorization: Bearer " + Wide(token) + L"\r\nContent-Type: application/json\r\n";
    if (!WinHttpSendRequest(request.value, headers.c_str(), static_cast<DWORD>(-1), body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) || !WinHttpReceiveResponse(request.value, nullptr)) throw std::runtime_error("Backend unavailable: Windows HTTP error " + std::to_string(GetLastError()));
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX) || status != 200) throw std::runtime_error("Backend returned HTTP " + std::to_string(status));
    std::string response;
    for (;;) {
        char buffer[4096]; DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &read)) throw std::runtime_error("Could not read backend response");
        if (!read) break;
        response.append(buffer, read);
        if (response.size() > 262144) throw std::runtime_error("Backend response exceeds protocol limit");
    }
    return response;
}
struct ScanState {
    std::mutex mutex;
    std::thread worker;
    std::atomic<bool> active{false}, cancel{false};
    size_t visited = 0;
    std::wstring current, message;
    struct Candidate { Game game; fs::path path, root; bool prepared, registered; std::wstring reason; };
    std::vector<Candidate> candidates;
    ~ScanState() { cancel = true; if (worker.joinable()) worker.join(); }
};
static ScanState scan;
static bool SameLocation(const fs::path& left, const fs::path& right) {
    std::error_code error;
    return _wcsicmp(left.c_str(), right.c_str()) == 0 || fs::equivalent(left, right, error);
}
static bool PreparedSource(const fs::path& directory, const fs::path& source) {
    auto marker = directory / "preparation-source.txt";
    if (!fs::is_regular_file(marker) || fs::file_size(marker) > 65536) return false;
    std::ifstream input(marker, std::ios::binary);
    std::string header, path; std::getline(input, header); std::getline(input, path);
    if (header.ends_with('\r')) header.pop_back();
    if (path.ends_with('\r')) path.pop_back();
    return header == "AnyPS5 Prepared Source 1" && !path.empty() && SameLocation(fs::path(Wide(path)), source);
}
static fs::path PreparedRoot(const Configuration& config) {
    auto setting = config.state / "preparation-root.txt";
    if (fs::is_regular_file(setting)) {
        if (fs::file_size(setting) > 65536) throw std::runtime_error("Invalid preparation folder setting");
        auto value = ReadFileText(setting);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
        fs::path root = Wide(value);
        if (!root.is_absolute()) throw std::runtime_error("Preparation folder must be absolute");
        return root;
    }
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &value))) throw std::runtime_error("Cannot find per-user preparation storage");
    fs::path result = fs::path(value) / "AnyPS5/PreparedGames"; CoTaskMemFree(value); return result;
}
struct PreparationState {
    HANDLE process = nullptr;
    DWORD pid = 0;
    bool active = false;
    fs::path job, output, source;
    std::wstring title, message;
    std::string status = "idle";
};
static PreparationState preparation;
static bool PreparationSupported(const Configuration& config) {
    return !config.engine.empty() && fs::is_regular_file(config.engine / "preparer/python/python.exe") && fs::is_regular_file(config.engine / "preparer/prepare.py");
}
static void SavePreparation(const Configuration& config) {
    auto temporary = config.state / "preparation/current.tmp";
    std::ofstream output(temporary, std::ios::binary); output.exceptions(std::ios::badbit | std::ios::failbit);
    output << "AnyPS5 Preparation 1\n" << preparation.pid << '\n' << std::quoted(Utf8(preparation.job.wstring())) << '\n'
           << std::quoted(Utf8(preparation.output.wstring())) << '\n' << std::quoted(Utf8(preparation.source.wstring())) << '\n'
           << std::quoted(Utf8(preparation.title));
    output.close();
    if (!MoveFileExW(temporary.c_str(), (config.state / "preparation/current.txt").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot preserve preparation tracking");
}
static void RestorePreparation(const Configuration& config) {
    auto record = config.state / "preparation/current.txt";
    if (!fs::exists(record)) return;
    std::ifstream input(record, std::ios::binary); std::string header, job, output, source, title;
    std::getline(input, header);
    if (header != "AnyPS5 Preparation 1" || !(input >> preparation.pid >> std::quoted(job) >> std::quoted(output) >> std::quoted(source) >> std::quoted(title))) throw std::runtime_error("Invalid preparation recovery record");
    preparation.job = Wide(job); preparation.output = Wide(output); preparation.source = Wide(source); preparation.title = Wide(title);
    if (!SameLocation(preparation.job.parent_path(), config.state / "preparation") || !preparation.output.is_absolute() || preparation.output.filename() != preparation.job.filename() || (!fs::is_regular_file(preparation.job / "done") && !SameLocation(preparation.output.parent_path(), PreparedRoot(config)))) throw std::runtime_error("Preparation recovery paths are invalid");
    preparation.active = true; preparation.status = "running"; preparation.message = L"Recovering previous preparation status";
    if (preparation.pid) preparation.process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, preparation.pid);
}
static void StartPreparation(const Configuration& config, const fs::path& source, const std::wstring& title) {
    if (preparation.active) throw std::runtime_error("A preparation is already running");
    if (!PreparationSupported(config)) throw std::runtime_error("Install the current engine bundle's desktop helper to enable automatic preparation");
    GUID identifier; if (FAILED(CoCreateGuid(&identifier))) throw std::runtime_error("Cannot allocate preparation identifier");
    wchar_t text[40]; StringFromGUID2(identifier, text, 40);
    preparation = {}; preparation.job = config.state / "preparation" / text; preparation.output = PreparedRoot(config) / text;
    preparation.source = source; preparation.title = title; preparation.active = true; preparation.status = "running";
    preparation.message = L"Starting preparation. Original files stay unchanged.";
    fs::create_directories(preparation.job); fs::create_directories(preparation.output.parent_path());
    SavePreparation(config);
    auto python = config.engine / "preparer/python/python.exe";
    auto command = Quote(python.wstring()) + L" -I -S -B " + Quote((config.engine / "preparer/prepare.py").wstring()) + L" --engine " + Quote(config.engine.wstring())
        + L" --input " + Quote(source.wstring()) + L" --output " + Quote(preparation.output.wstring()) + L" --job " + Quote(preparation.job.wstring());
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION info{};
    if (!CreateProcessW(python.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, config.engine.c_str(), &startup, &info)) {
        preparation.active = false; preparation.status = "failed"; preparation.message = L"Preparation worker could not start";
        throw std::runtime_error("Cannot start the bundled preparation worker: Windows error " + std::to_string(GetLastError()));
    }
    CloseHandle(info.hThread); preparation.process = info.hProcess; preparation.pid = info.dwProcessId; SavePreparation(config);
}
static std::string PreparationSnapshot(const Configuration& config) {
    std::string detail = "{}";
    try {
        auto status = preparation.job / "status.json";
        if (!preparation.job.empty() && fs::is_regular_file(status) && fs::file_size(status) < 65536) {
            auto value = ReadFileText(status);
            if (!value.empty() && value.front() == '{' && value.back() == '}') detail = std::move(value);
        }
    } catch (const std::exception&) {}
    auto root = PreparedRoot(config);
    std::error_code spaceError;
    auto space = fs::space(root, spaceError);
    return "{\"storage_supported\":true,\"root\":" + Json(root.wstring()) + ",\"free_bytes\":" + (spaceError ? "null" : std::to_string(space.available))
        + ",\"supported\":" + std::string(PreparationSupported(config) ? "true" : "false") + ",\"active\":" + (preparation.active ? "true" : "false")
        + ",\"status\":\"" + preparation.status + "\",\"title\":" + Json(preparation.title) + ",\"message\":" + Json(preparation.message)
        + ",\"output\":" + Json(preparation.output.wstring()) + ",\"detail\":" + detail + '}';
}
static void PollPreparation(const Configuration& config, std::vector<std::wstring>& notes) {
    if (!preparation.active) return;
    auto done = preparation.job / "done";
    if (!fs::is_regular_file(done)) {
        if (preparation.process && WaitForSingleObject(preparation.process, 0) == WAIT_TIMEOUT) return;
        std::ofstream(preparation.job / "cancel") << "cancel";
        preparation.status = "failed"; preparation.message = L"Preparation worker stopped before completion. No output was added; check its log and retry.";
    } else {
        auto outcome = ReadFileText(done);
        if (outcome == "succeeded") {
            try {
                InstanceLock lock(config.collection);
                auto games = ReadLauncherLibrary(config.collection);
                auto game = MakeGame(config.collection, preparation.output / "app.exe");
                game.title = preparation.title;
                if (AddGame(games, config.collection, game)) SaveLauncherLibrary(config.collection, games);
                preparation.status = "succeeded"; preparation.message = L"Prepared and added " + preparation.title + L". Gameplay is unverified; choose Play to test it.";
                std::lock_guard scanLock(scan.mutex);
                for (auto& candidate : scan.candidates) if (SameLocation(candidate.path, preparation.source)) candidate.registered = true;
            } catch (const std::exception& error) {
                preparation.message = L"Windows copy prepared; waiting to add it to the library. " + Wide(error.what());
                preparation.status = "registering"; return;
            }
        } else {
            preparation.status = outcome == "cancelled" ? "cancelled" : "failed";
            preparation.message = outcome == "cancelled" ? L"Preparation cancelled. Original files were not changed." : L"Preparation failed. No game was added; see the error and log below.";
        }
    }
    if (preparation.process) CloseHandle(preparation.process);
    preparation.process = nullptr; preparation.active = false;
    notes.push_back(preparation.message + L" Output: " + preparation.output.wstring());
}
static void StartScan(const Configuration& config, const std::vector<fs::path>& roots, const std::vector<Game>& games) {
    if (scan.active) throw std::runtime_error("A scan is already active");
    if (scan.worker.joinable()) scan.worker.join();
    {
        std::lock_guard lock(scan.mutex);
        scan.candidates.clear(); scan.visited = 0; scan.current.clear(); scan.message = L"Scanning folders";
    }
    scan.cancel = false; scan.active = true;
    scan.worker = std::thread([config, roots, games] {
        try {
            size_t total = 0;
            for (const auto& root : roots) {
                auto discoveries = ScanLibraryFolder(root, [&](size_t, const fs::path& path) {
                    std::lock_guard lock(scan.mutex); scan.visited = ++total; scan.current = path.wstring();
                    return !scan.cancel;
                });
                for (const auto& item : discoveries) {
                    if (scan.cancel) break;
                    auto path = item.prepared ? item.path.parent_path() : item.path;
                    Game game;
                    if (item.prepared) game = MakeGame(config.collection, item.path);
                    else game = DescribeLauncherGame(config.collection, path);
                    bool registered = false;
                    for (const auto& existing : games) if (SameLocation(Resolve(config.collection, existing.workingDirectory), path) || (!item.prepared && PreparedSource(Resolve(config.collection, existing.workingDirectory), path))) { registered = true; game = existing; break; }
                    std::lock_guard lock(scan.mutex);
                    if (scan.candidates.size() >= 4096) throw std::runtime_error("Too many results. Choose a more specific folder.");
                    if (std::none_of(scan.candidates.begin(), scan.candidates.end(), [&](const auto& candidate) { return SameLocation(candidate.path, path); }))
                        scan.candidates.push_back({game, fs::canonical(path), fs::canonical(root), item.prepared, registered, item.reason});
                }
                if (scan.cancel) break;
            }
            std::lock_guard lock(scan.mutex);
            if (scan.cancel) { scan.candidates.clear(); scan.message = L"Scan cancelled. No games were added."; }
            else scan.message = scan.candidates.empty() ? L"No games found. Choose a game folder or a parent containing prepared games." : L"Scan complete. Review the results and add selected games.";
        } catch (const std::exception& error) {
            std::lock_guard lock(scan.mutex); scan.candidates.clear(); scan.message = Wide(error.what());
        }
        scan.active = false;
    });
}
static std::string ScanSnapshot() {
    std::lock_guard lock(scan.mutex);
    std::ostringstream output;
    output << "{\"active\":" << (scan.active ? "true" : "false") << ",\"visited\":" << scan.visited << ",\"current\":" << Json(scan.current) << ",\"message\":" << Json(scan.message) << ",\"results\":[";
    bool separator = false;
    if (!scan.active) for (const auto& item : scan.candidates) {
        if (separator) output << ','; separator = true;
        output << "{\"id\":" << Json(item.game.id) << ",\"title\":" << Json(item.game.title) << ",\"title_id\":" << Json(item.game.titleId) << ",\"path\":" << Json(item.path.wstring()) << ",\"prepared\":" << (item.prepared ? "true" : "false") << ",\"registered\":" << (item.registered ? "true" : "false") << ",\"reason\":" << Json(item.reason) << '}';
    }
    output << "]}"; return output.str();
}
static std::string Artwork(const fs::path& directory) {
    auto path = directory / "app0/sce_sys/icon0.png";
    if (!fs::is_regular_file(path) || fs::file_size(path) > 512 * 1024) return {};
    std::ifstream input(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(input)), {}), result;
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned value = 0; int bits = -6;
    for (unsigned char ch : bytes) { value = (value << 8) | ch; bits += 8; while (bits >= 0) { result += alphabet[(value >> bits) & 63]; bits -= 6; } }
    if (bits > -6) result += alphabet[((value << 8) >> (bits + 8)) & 63];
    while (result.size() % 4) result += '=';
    return result;
}
static std::set<std::wstring> uncertain;
static std::map<std::wstring, std::pair<DWORD, std::wstring>> lastResults;
static void SaveActiveSessions(const Configuration& config, const std::map<std::wstring, std::unique_ptr<Session>>& sessions) {
    auto temp = config.state / "active-sessions.tmp";
    std::ofstream output(temp, std::ios::binary); output.exceptions(std::ios::badbit | std::ios::failbit);
    for (const auto& [id, session] : sessions) output << Utf8(id) << '\n';
    for (const auto& id : uncertain) output << Utf8(id) << '\n';
    output.close();
    if (!MoveFileExW(temp.c_str(), (config.state / "active-sessions.txt").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot preserve active session tracking");
}
static std::string Snapshot(const Configuration& config, const std::map<std::wstring, std::unique_ptr<Session>>& sessions, const std::vector<std::wstring>& notes, const std::string& result) {
    std::ostringstream output;
    try {
        InstanceLock lock(config.collection);
        auto games = ReadLauncherLibrary(config.collection);
        auto folders = ReadLibraryFolders(config.collection);
        output << "{\"busy\":false,\"message\":\"Ready. Games run on Windows.\",\"games\":[";
        bool separator = false;
        for (const auto& game : games) {
            if (separator) output << ','; separator = true;
            output << "{\"exit_code\":" << (lastResults.contains(game.id) ? std::to_string(lastResults.at(game.id).first) : "null") << ",\"log_directory\":" << Json(lastResults.contains(game.id) ? lastResults.at(game.id).second : L"") << ",\"id\":" << Json(game.id) << ",\"title\":" << Json(game.title) << ",\"title_id\":" << Json(game.titleId) << ",\"status\":" << Json(game.status) << ",\"working_directory\":" << Json(Resolve(config.collection, game.workingDirectory).wstring()) << ",\"available\":" << (fs::is_directory(Resolve(config.collection, game.workingDirectory)) && fs::is_regular_file(Resolve(config.collection, game.executable)) ? "true" : "false") << ",\"executable\":" << Json(Resolve(config.collection, game.executable).wstring()) << ",\"artwork\":" << Json(Wide(output.tellp() < 3 * 1024 * 1024 ? Artwork(Resolve(config.collection, game.workingDirectory)) : std::string{})) << ",\"uncertain\":" << (uncertain.contains(game.id) ? "true" : "false") << ",\"running\":" << (sessions.contains(game.id) ? "true" : "false") << '}';
        }
        output << "],\"folders\":["; separator = false;
        for (const auto& folder : folders) {
            if (separator) output << ','; separator = true;
            auto path = Resolve(config.collection, folder.path);
            output << "{\"id\":" << Json(folder.id) << ",\"path\":" << Json(path.wstring()) << ",\"exists\":" << (fs::is_directory(path) ? "true" : "false") << '}';
        }
        output << ']';
    } catch (const std::exception& error) {
        output.str(""); output.clear();
        output << "{\"busy\":true,\"games\":[],\"folders\":[],\"message\":" << Json(L"Native launcher owns the collection, or its data needs attention. " + Wide(error.what()));
    }
    output << ",\"helper_protocol\":1,\"helper_revision\":\"" << ANYPS5_HELPER_REVISION << "\",\"preparation\":" << PreparationSnapshot(config) << ",\"scan\":" << ScanSnapshot();
    output << ",\"notes\":[";
    for (size_t i = 0; i < notes.size(); ++i) { if (i) output << ','; output << Json(notes[i]); }
    output << ']';
    if (!result.empty()) output << ",\"result\":" << result;
    output << '}'; return output.str();
}
static std::string Execute(const Configuration& config, const std::string& command, std::map<std::wstring, std::unique_ptr<Session>>& sessions, std::vector<std::wstring>& notes) {
    auto first = command.find('\t'), second = command.find('\t', first == std::string::npos ? 0 : first + 1);
    if (first == std::string::npos || second == std::string::npos || command.find('\t', second + 1) != std::string::npos) throw std::runtime_error("Malformed backend command");
    auto id = command.substr(0, first), verb = command.substr(first + 1, second - first - 1), argument = command.substr(second + 1);
    GUID guid;
    if (FAILED(CLSIDFromString(Wide("{" + id + "}").c_str(), &guid))) throw std::runtime_error("Invalid command identifier");
    bool ok = false; std::wstring message;
    try {
        if (preparation.active && (verb == "scan" || verb == "rescan" || verb == "pick_folder" || verb == "remove_root" || verb == "prepare_scan" || verb == "set_preparation_root")) throw std::runtime_error("Wait for preparation to finish or cancel it first");
        if (verb == "set_preparation_root") {
            auto root = NormalizeLibraryPath(Wide(argument));
            auto temporary = config.state / "preparation-root.pending";
            std::ofstream output(temporary, std::ios::binary); output.exceptions(std::ios::badbit | std::ios::failbit);
            output << Utf8(root.wstring()); output.close();
            if (!MoveFileExW(temporary.c_str(), (config.state / "preparation-root.txt").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot save preparation folder");
            return "{\"id\":" + Json(Wide(id)) + ",\"ok\":true,\"message\":\"Preparation folder saved for new copies. Existing games stay in place.\"}";
        }
        if (verb == "cancel_preparation") {
            if (!argument.empty()) throw std::runtime_error("Cancellation accepts no path");
            if (preparation.active) std::ofstream(preparation.job / "cancel") << "cancel";
            return "{\"id\":" + Json(Wide(id)) + ",\"ok\":true,\"message\":\"Cancellation requested; already published copies are retained.\"}";
        }
        if (verb == "open_preparation_log") {
            if (!argument.empty() || preparation.job.empty()) throw std::runtime_error("No preparation log is available");
            auto log = preparation.job / "preparation.log";
            if (!fs::is_regular_file(log)) throw std::runtime_error("The preparation log has not been created yet");
            wchar_t system[MAX_PATH]; GetSystemDirectoryW(system, MAX_PATH);
            auto application = fs::path(system) / "notepad.exe";
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", application.c_str(), Quote(log.wstring()).c_str(), nullptr, SW_SHOWNORMAL)) <= 32) throw std::runtime_error("Cannot open the preparation log");
            return "{\"id\":" + Json(Wide(id)) + ",\"ok\":true,\"message\":\"Preparation log opened on Windows.\"}";
        }
        fs::path picked;
        if (verb == "pick_folder") {
            if (!argument.empty()) throw std::runtime_error("Folder picker accepts no path from the backend");
            HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP);
            if (!desktop) throw std::runtime_error("Unlock the Windows desktop before selecting a folder");
            CloseDesktop(desktop);
            picked = PickLibraryFolder();
        }
        InstanceLock lock(config.collection);
        auto games = ReadLauncherLibrary(config.collection);
        auto folders = ReadLibraryFolders(config.collection);
        if (verb == "launch") {
            if (!sessions.empty() || !uncertain.empty()) throw std::runtime_error("A game is running or its status needs confirmation in game details");
            auto found = std::find_if(games.begin(), games.end(), [&](const Game& game) { return game.id == Wide(argument); });
            if (found == games.end()) throw std::runtime_error("Game ID is not registered");
            uncertain.insert(found->id);
            SaveActiveSessions(config, sessions);
            try { sessions[found->id] = std::make_unique<Session>(config.collection, *found); }
            catch (...) { uncertain.erase(found->id); SaveActiveSessions(config, sessions); throw; }
            uncertain.erase(found->id);
            lastResults.erase(found->id);
            SaveActiveSessions(config, sessions);
            message = L"Launch process created for " + found->title + L". Gameplay is unverified. Session logs: " + sessions[found->id]->Folder().wstring();
        } else if (verb == "pick_folder") {
            if (picked.empty()) message = L"Folder selection cancelled";
            else {
                bool added = AddLibraryFolder(folders, config.collection, picked);
                if (added) SaveLibraryFolders(config.collection, folders);
                message = added ? L"Library folder saved. Rescan to discover prepared games." : L"Library folder already saved; no duplicate added.";
            }
        } else if (verb == "acknowledge_exit") {
            if (!uncertain.erase(Wide(argument))) throw std::runtime_error("This game has no uncertain session");
            SaveActiveSessions(config, sessions);
            message = L"Previous game closure confirmed by user";
        } else if (verb == "scan") {
            if (scan.active) throw std::runtime_error("Cancel the active scan first");
            auto path = NormalizeLibraryPath(Wide(argument));
            if (AddLibraryFolder(folders, config.collection, path)) SaveLibraryFolders(config.collection, folders);
            StartScan(config, {path}, games);
            message = L"Scan started. Linking keeps files in place.";
        } else if (verb == "cancel_scan") {
            scan.cancel = true; message = L"Cancellation requested";
        } else if (verb == "prepare_scan") {
            if (scan.active) throw std::runtime_error("Wait for the scan to finish");
            std::lock_guard scanLock(scan.mutex);
            auto item = std::find_if(scan.candidates.begin(), scan.candidates.end(), [&](const auto& candidate) { return candidate.game.id == Wide(argument); });
            if (item == scan.candidates.end() || item->prepared || item->registered) throw std::runtime_error("This scan result is no longer available for preparation. Scan again.");
            if (_wcsicmp(fs::canonical(item->path).c_str(), item->path.c_str()) != 0) throw std::runtime_error("The source folder changed. Scan again.");
            auto relative = fs::canonical(item->path).lexically_relative(fs::canonical(item->root));
            if (relative.empty() || *relative.begin() == L".." || !std::any_of(folders.begin(), folders.end(), [&](const auto& folder) { return SameLocation(Resolve(config.collection, folder.path), item->root); })) throw std::runtime_error("The preparation source is outside the saved discovery root");
            StartPreparation(config, item->path, item->game.title);
            message = L"Preparation started. A separate Windows copy will be added when validation succeeds.";
        } else if (verb == "add_scan") {
            if (scan.active) throw std::runtime_error("Wait for the scan to finish");
            std::lock_guard scanLock(scan.mutex);
            std::istringstream selected(argument); std::string selectedId; size_t added = 0;
            while (std::getline(selected, selectedId, ',')) {
                auto item = std::find_if(scan.candidates.begin(), scan.candidates.end(), [&](const auto& candidate) { return candidate.game.id == Wide(selectedId); });
                if (item == scan.candidates.end() || !item->prepared) throw std::runtime_error("Scan result expired or needs preparation. Scan again.");
                if (_wcsicmp(fs::canonical(item->path).c_str(), item->path.c_str()) != 0) throw std::runtime_error("Folder changed since discovery. Scan again.");
                auto relative = fs::canonical(item->path).lexically_relative(fs::canonical(item->root));
                if (relative.empty() || *relative.begin() == L"..") throw std::runtime_error("Game is outside the authorized root");
                if (!std::any_of(folders.begin(), folders.end(), [&](const auto& folder) { return SameLocation(Resolve(config.collection, folder.path), item->root); })) throw std::runtime_error("Library root was removed. Scan again.");
                if (std::none_of(games.begin(), games.end(), [&](const auto& game) { return SameLocation(Resolve(config.collection, game.workingDirectory), item->path); })) {
                    auto fresh = MakeGame(config.collection, Resolve(config.collection, item->game.executable));
                    added += AddGame(games, config.collection, fresh);
                }
            }
            SaveLauncherLibrary(config.collection, games);
            for (auto& item : scan.candidates) item.registered = std::any_of(games.begin(), games.end(), [&](const auto& game) { return SameLocation(Resolve(config.collection, game.workingDirectory), item.path); });
            message = std::to_wstring(added) + L" games added. Existing launch settings preserved.";
        } else if (verb == "rescan") {
            if (!argument.empty()) throw std::runtime_error("Rescan accepts no backend path");
            std::vector<fs::path> roots;
            for (const auto& folder : folders) roots.push_back(Resolve(config.collection, folder.path));
            StartScan(config, roots, games);
            message = L"Rescan started. Review discovered games before adding.";
        } else if (verb == "remove_root") {
            auto previous = folders.size();
            std::erase_if(folders, [&](const LibraryFolder& folder) { return folder.id == Wide(argument); });
            if (previous == folders.size()) throw std::runtime_error("Library folder ID is not registered");
            SaveLibraryFolders(config.collection, folders);
            message = L"Library root removed. Registered games and saves are unchanged.";
        } else throw std::runtime_error("Unsupported command");
        ok = true;
    } catch (const std::exception& error) { message = Wide(error.what()); }
    Record(config, verb + ": " + Utf8(message));
    return "{\"id\":" + Json(Wide(id)) + ",\"ok\":" + (ok ? "true" : "false") + ",\"message\":" + Json(message) + '}';
}
static int RunDesktopHelper(const Configuration& config) {
    DWORD sessionId = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) || !sessionId) throw std::runtime_error("Desktop helper must run in the signed-in user's desktop session, not as a service");
    InstanceLock helperLock(config.state);
    std::map<std::wstring, std::unique_ptr<Session>> sessions;
    std::vector<std::wstring> notes;
    try { RestorePreparation(config); } catch (const std::exception& error) { notes.push_back(Wide(error.what())); }
    if (fs::exists(config.state / "active-sessions.txt")) {
        std::istringstream input(ReadFileText(config.state / "active-sessions.txt")); std::string id;
        while (std::getline(input, id)) { GUID guid; if (SUCCEEDED(CLSIDFromString(Wide(id).c_str(), &guid))) uncertain.insert(Wide(id)); }
        if (!uncertain.empty()) notes.push_back(L"The helper restarted during a game session. Games were not terminated. Confirm closure in the affected game's details before launching again.");
    }
    std::string pending, lastError;
    bool connected = false;
    Record(config, "Desktop helper started in user session " + std::to_string(sessionId));
    for (;;) {
        try { PollPreparation(config, notes); } catch (const std::exception& error) { preparation.message = Wide(error.what()); }
        for (auto it = sessions.begin(); it != sessions.end();) {
            auto code = it->second->Poll();
            if (code) { lastResults[it->first] = {*code, it->second->Folder().wstring()}; notes.push_back(L"Launch process exited with code " + std::to_wstring(*code) + L". See " + it->second->Folder().wstring()); it = sessions.erase(it); SaveActiveSessions(config, sessions); }
            else ++it;
        }
        if (notes.size() > 100) notes.erase(notes.begin(), notes.end() - 100);
        try {
            Request(config, L"POST", L"/agent/report", Snapshot(config, sessions, notes, pending));
            pending.clear();
            if (!connected) Record(config, "Connected to loopback backend");
            connected = true; lastError.clear();
            auto command = Request(config, L"GET", L"/agent/poll");
            if (!command.empty()) pending = Execute(config, command, sessions, notes);
        } catch (const std::exception& error) {
            connected = false;
            if (lastError != error.what()) { lastError = error.what(); Record(config, lastError + "; retrying automatically"); }
        }
        Sleep(scan.active ? 100 : preparation.active ? 500 : 2000);
    }
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        int count; auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
        std::vector<std::wstring> args(raw + 1, raw + count); LocalFree(raw);
        if (args.size() == 3 && args[0] == L"--initialize") {
            fs::path root = args[1]; InstanceLock lock(root);
            auto folders = ReadLibraryFolders(root); if (AddLibraryFolder(folders, root, args[2])) SaveLibraryFolders(root, folders);
            auto games = ReadLauncherLibrary(root); auto previous = games.size(); RescanLibrary(root, games, folders); if (games.size() != previous) SaveLauncherLibrary(root, games);
            return 0;
        }
        if (args.size() == 2 && args[0] == L"--pair") {
            auto config = ReadConfiguration(args[1]);
            auto code = Request(config, L"POST", L"/agent/pair", "{}");
            std::cout << code;
            return 0;
        }
        if (args.size() == 2 && args[0] == L"--validate-config") {
            auto config = ReadConfiguration(args[1]);
            auto token = ReadFileText(config.token);
            while (!token.empty() && (token.back() == '\r' || token.back() == '\n')) token.pop_back();
            if (token.size() < 32 || token.size() > 256) throw std::runtime_error("Invalid pairing credential");
            return 0;
        }
        if (args.size() != 2 || args[0] != L"--config") throw std::runtime_error("Use --config helper.conf, or --initialize collection library-folder");
        auto config = ReadConfiguration(args[1]);
        return RunDesktopHelper(config);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

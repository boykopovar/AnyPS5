#include "ShaderCacheDirectory.hpp"
#include <iostream>
#include <new>
#include <stdexcept>

static bool FailLargeAllocation = false;

void* operator new(std::size_t size) {
    if (FailLargeAllocation && size > 1024 * 1024) throw std::bad_alloc();
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

static void Require(bool ok, const char* message = "Cache file read failed") { if (!ok) throw std::runtime_error(message); }

#ifdef _WIN32
static void RemoveCachePaths(const std::filesystem::path& path) {
    WIN32_FIND_DATAW data{};
    const auto search = FindFirstFileW((path / L"*").c_str(), &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..") continue;
            const auto child = path / name;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveCachePaths(child);
            else DeleteFileW(child.c_str());
        } while (FindNextFileW(search, &data));
        FindClose(search);
    }
    RemoveDirectoryW(path.c_str());
}

static void VerifyCachePaths(const std::filesystem::path& root) {
    const auto extended = [](const std::filesystem::path& path) {
        auto native = path;
        native.make_preferred();
        const auto& text = native.native();
        if (text.starts_with(L"\\\\")) return std::filesystem::path(L"\\\\?\\UNC\\" + text.substr(2));
        return std::filesystem::path(L"\\\\?\\" + text);
    };
    const std::vector<std::byte> original{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const std::vector<std::byte> replacement{std::byte{5}, std::byte{6}};
    const auto verify = [&](const std::filesystem::path& path) {
        std::filesystem::create_directories(extended(path).parent_path());
        const bool retainedReplace = !ShaderRecompiler::PosixReplaceUnsupported(path.parent_path());
        if (!retainedReplace) std::cerr << "cache file read: no POSIX rename (ERROR_INVALID_PARAMETER) under the test directory; skipping replacement with a retained reader\n";
        for (const auto& alias : {path, extended(path)}) {
            Require(ShaderRecompiler::WriteFileAtomically(alias, original), "cannot write cache path");
            std::vector<std::byte> bytes;
            Require(ShaderRecompiler::ReadWholeFile(path, bytes) && bytes == original, "cannot read ordinary cache path");
            Require(ShaderRecompiler::ReadWholeFile(extended(path), bytes) && bytes == original, "cannot read extended cache path");
            if (!retainedReplace) {
                Require(ShaderRecompiler::WriteFileAtomically(alias, replacement), "cannot replace cache path");
                Require(ShaderRecompiler::ReadWholeFile(alias, bytes) && bytes == replacement, "replacement cache bytes do not match");
                continue;
            }
            const auto old = CreateFileW(extended(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(old != INVALID_HANDLE_VALUE, "cannot retain the original cache reader");
            struct Close { HANDLE file; ~Close() { CloseHandle(file); } } close{old};
            Require(ShaderRecompiler::WriteFileAtomically(alias, replacement), "cannot replace cache path with retained reader");
            Require(ShaderRecompiler::ReadWholeFile(alias, bytes) && bytes == replacement, "replacement cache bytes do not match");
            std::byte retained[4]{};
            DWORD count = 0;
            Require(ReadFile(old, retained, sizeof(retained), &count, nullptr) && count == original.size(), "cannot read the retained cache handle");
            Require(std::equal(original.begin(), original.end(), retained), "retained cache bytes changed after replacement");
        }
        WIN32_FIND_DATAW data{};
        const auto temporary = FindFirstFileW((extended(path).parent_path() / L"*.tmp.*").c_str(), &data);
        if (temporary != INVALID_HANDLE_VALUE) FindClose(temporary);
        Require(temporary == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "cache temporary file was left behind");
    };
    auto directory = root / "paths";
    while (directory.native().size() < 150) directory /= std::wstring(std::min<std::size_t>(40, 150 - directory.native().size()), L'd');
    for (const auto length : {180u, 235u, 269u, 320u}) {
        const auto size = std::max<std::size_t>(length, directory.native().size() + 12);
        verify(directory / (std::wstring(size - directory.native().size() - 5, L'x') + L".bin"));
    }
    auto deep = root;
    while (deep.native().size() < 300) deep /= std::wstring(40, L'd');
    verify(deep / L"cache \u00e4\u6f22.bin");
    std::filesystem::create_directories(root / "relative");
    struct RestoreDirectory {
        std::filesystem::path previous = std::filesystem::current_path();
        ~RestoreDirectory() {
            std::error_code error;
            std::filesystem::current_path(previous, error);
        }
    } restoreDirectory;
    std::filesystem::current_path(root);
    auto relative = (root / "relative" / ".." / "relative.bin").lexically_relative(std::filesystem::current_path());
    Require(!relative.empty(), "cannot construct the cache relative path");
    Require(ShaderRecompiler::WriteFileAtomically(relative, original), "cannot write the cache relative path");
    std::vector<std::byte> bytes;
    Require(ShaderRecompiler::ReadWholeFile(relative, bytes) && bytes == original, "cannot read the cache relative path");
    const auto missing = deep / "missing.bin";
    Require(!ShaderRecompiler::ReadWholeFile(missing, bytes) && bytes.empty(), "missing cache path did not clear the read buffer");
}
#endif

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5-cache-read-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto directory = path;
    directory += "-paths";
    int status = 0;
    try {
        std::vector<std::byte> bytes{std::byte{1}};
        Require(!ShaderRecompiler::ReadWholeFile(path, bytes) && bytes.empty());
        { std::ofstream file(path, std::ios::binary); }
        Require(ShaderRecompiler::ReadWholeFile(path, bytes) && bytes.empty());
        const std::vector<char> payload(2 * 1024 * 1024, 'S');
        { std::ofstream file(path, std::ios::binary | std::ios::trunc); file.write(payload.data(), payload.size()); Require(static_cast<bool>(file)); }
        Require(ShaderRecompiler::ReadWholeFile(path, bytes) && bytes.size() == payload.size());
        Require(bytes.front() == std::byte{'S'} && bytes.back() == std::byte{'S'});
        std::vector<std::byte> failed{std::byte{1}};
        FailLargeAllocation = true;
        const auto read = ShaderRecompiler::ReadWholeFile(path, failed);
        FailLargeAllocation = false;
        Require(!read && failed.empty());
        std::cout << "5 cache file read cases passed\n";
#ifdef _WIN32
        VerifyCachePaths(directory);
        std::cout << "Windows cache path and retained-reader replacement cases passed\n";
#endif
    } catch (const std::exception& error) { FailLargeAllocation = false; std::cerr << error.what() << '\n'; status = 1; }
    std::error_code error;
    std::filesystem::remove(path, error);
#ifdef _WIN32
    RemoveCachePaths(std::filesystem::path(L"\\\\?\\" + directory.native()));
#endif
    return status;
}

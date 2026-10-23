#include "prx/libkernel/File/include/WindowsExtendedPath.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "check at line %d failed\n", line); throw std::runtime_error("Windows extended path check failed"); } }
#define Require(value) Check((value), __LINE__)

static std::wstring Extend(const std::wstring& input) { return File::WindowsExtendedPath(std::filesystem::path(input)).native(); }

class TemporaryDirectory {
public:
    TemporaryDirectory() : original(std::filesystem::current_path()) {
        const auto base = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 16 && root.empty(); ++attempt) {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto candidate = base / (L"aps5_extended_path_" + std::to_wstring(stamp) + L"_" + std::to_wstring(attempt));
            if (std::filesystem::create_directory(candidate)) root = candidate;
        }
        if (root.empty()) throw std::runtime_error("Windows extended path test could not create a temporary directory");
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::current_path(original, error);
        std::filesystem::remove_all(root, error);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    const std::filesystem::path& Path() const { return root; }

private:
    std::filesystem::path original;
    std::filesystem::path root;
};

static void CheckRelativeUnderLongDirectory() {
    const std::wstring file = L"file_" + std::wstring(30, L'x');
    if (std::filesystem::current_path().native().size() + file.size() + 1 < 260) Require(Extend(file) == file);
    const TemporaryDirectory temporary;
    const std::size_t target = 240;
    Require(temporary.Path().native().size() + 1 < target);
    const auto directory = temporary.Path() / std::wstring(target - temporary.Path().native().size() - 1, L'd');
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    const std::wstring current = std::filesystem::current_path().native();
    Require(current.size() + file.size() + 1 >= 260);
    Require(Extend(file) == L"\\\\?\\" + current + L"\\" + file);
}

int main() {
    const std::wstring name(300, L'a');
    const std::wstring shortPath = L"C:\\games\\title\\file.bundle";
    Require(Extend(shortPath) == shortPath);
    Require(Extend(L"C:\\" + name) == L"\\\\?\\C:\\" + name);
    Require(Extend(L"C:/dir/" + name) == L"\\\\?\\C:\\dir\\" + name);
    Require(Extend(L"C:\\dir\\.\\sub\\..\\" + name) == L"\\\\?\\C:\\dir\\" + name);
    Require(Extend(L"\\\\server\\share\\" + name) == L"\\\\?\\UNC\\server\\share\\" + name);
    Require(Extend(L"\\\\?\\C:\\" + name) == L"\\\\?\\C:\\" + name);
    Require(Extend(L"\\\\?\\UNC\\server\\share\\" + name) == L"\\\\?\\UNC\\server\\share\\" + name);
    Require(Extend(L"\\\\.\\pipe\\" + name) == L"\\\\.\\pipe\\" + name);
    const std::wstring shrinks = L"C:\\" + std::wstring(200, L'a') + L"\\..\\" + std::wstring(200, L'b');
    Require(Extend(shrinks) == shrinks);
    const std::wstring relative = Extend(L"relative\\" + name);
    const std::wstring current = std::filesystem::current_path().native();
    Require(relative == L"\\\\?\\" + current + L"\\relative\\" + name);
    CheckRelativeUnderLongDirectory();
}

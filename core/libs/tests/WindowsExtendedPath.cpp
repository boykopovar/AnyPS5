#include "prx/libkernel/File/include/WindowsExtendedPath.hpp"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "check at line %d failed\n", line); throw std::runtime_error("Windows extended path check failed"); } }
#define Require(value) Check((value), __LINE__)

static std::wstring Extend(const std::wstring& input) { return File::WindowsExtendedPath(std::filesystem::path(input)).native(); }

static void CheckRelativeUnderLongDirectory() {
    const auto original = std::filesystem::current_path();
    const std::wstring file = L"file_" + std::wstring(30, L'x');
    if (original.native().size() + file.size() + 1 < 260) Require(Extend(file) == file);
    const auto root = std::filesystem::temp_directory_path() / L"aps5_extended_path_test";
    const std::size_t target = 240;
    Require(root.native().size() + 1 < target);
    const auto directory = root / std::wstring(target - root.native().size() - 1, L'd');
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    const std::wstring current = std::filesystem::current_path().native();
    const std::wstring extended = Extend(file);
    std::filesystem::current_path(original);
    std::filesystem::remove_all(root);
    Require(current.size() + file.size() + 1 >= 260);
    Require(extended == L"\\\\?\\" + current + L"\\" + file);
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

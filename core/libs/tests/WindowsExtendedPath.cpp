#include "prx/libkernel/File/include/WindowsExtendedPath.hpp"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "check at line %d failed\n", line); throw std::runtime_error("Windows extended path check failed"); } }
#define Require(value) Check((value), __LINE__)

static std::wstring Extend(const std::wstring& input) { return File::WindowsExtendedPath(std::filesystem::path(input)).native(); }

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
}

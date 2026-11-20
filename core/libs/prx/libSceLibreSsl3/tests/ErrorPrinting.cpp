#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "error printing check failed at line %d\n", line);
    std::abort();
}

std::vector<std::string> capturedLines;

int APS5_VABI Capture(const char* text, std::size_t length, void*) {
    capturedLines.emplace_back(text, length);
    return 1;
}

void SeedDistinctErrors() {
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    LibreSslPutError_nid_no_patch(3, 100, 65);
    LibreSslPutError_nid_no_patch(4, 101, 66);
}

}

int main() {
    ERR_clear_error_nid_postfix();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = "err_print_" + std::to_string(stamp);
    Require(std::filesystem::create_directory(directory));
    const std::string path = (directory / "errors.txt").generic_string();
    FileStream* stream = fopen_nid_postfix(path.c_str(), "w+");
    Require(stream != nullptr);
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    char line[512] {};
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) != nullptr);
    Require(std::strstr(line, "error:") != nullptr);
    Require(ERR_peek_error_nid_postfix() == 0);
    LibreSslPutError_nid_no_patch(20, 0xfff, 67);
    capturedLines.clear();
    ERR_print_errors_cb_nid_postfix(Capture, nullptr);
    Require(capturedLines.size() == 1);
    Require(std::strcmp(line, capturedLines[0].c_str()) == 0);
    Require(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix(path.c_str(), "w+");
    Require(stream != nullptr);
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) == nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    stream = fopen_nid_postfix(path.c_str(), "w+");
    Require(stream != nullptr);
    SeedDistinctErrors();
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(ERR_peek_error_nid_postfix() == 0);
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    std::vector<std::string> fileLines;
    while (fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) != nullptr) fileLines.emplace_back(line);
    Require(fileLines.size() == 3);
    Require(fclose_nid_postfix(stream) == 0);

    SeedDistinctErrors();
    capturedLines.clear();
    ERR_print_errors_cb_nid_postfix(Capture, nullptr);
    Require(capturedLines.size() == 3);
    for (std::size_t index = 0; index < fileLines.size(); ++index)
        Require(fileLines[index] == capturedLines[index]);

    stream = fopen_nid_postfix(path.c_str(), "w+");
    Require(stream != nullptr);
    ERR_print_errors_fp_nid_postfix(reinterpret_cast<std::FILE*>(stream));
    Require(fflush_nid_postfix(stream) == 0);
    rewind_nid_postfix(stream);
    Require(fgets_nid_postfix(line, static_cast<int>(sizeof(line)), stream) == nullptr);
    Require(fclose_nid_postfix(stream) == 0);

    bool threw = false;
    try { ERR_print_errors_fp_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    Require(remove_nid_postfix(path.c_str()) == 0);
    Require(std::filesystem::remove(directory));

    return 0;
}

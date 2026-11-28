#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "prx/libc/include/general/VabiMacros.hpp"

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

extern "C" {
#if defined(_WIN32)
std::string BacktraceModulePath_nid_no_patch(HMODULE handle, std::size_t initialCapacity);
#endif
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "print backtrace: %s\n", message);
    std::abort();
}

std::string ReadAll(std::FILE* file) {
    std::rewind(file);
    std::string text;
    char buffer[4096];
    for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
    return text;
}

struct Streams {
    std::string out;
    std::string err;
};

Streams Capture(void (*body)()) {
    std::fflush(stdout);
    std::fflush(stderr);
    std::FILE* outFile = std::tmpfile();
    std::FILE* errFile = std::tmpfile();
    Require(outFile != nullptr && errFile != nullptr, "cannot create capture files");
#if defined(_WIN32)
    const int savedOut = _dup(_fileno(stdout));
    const int savedErr = _dup(_fileno(stderr));
    _dup2(_fileno(outFile), _fileno(stdout));
    _dup2(_fileno(errFile), _fileno(stderr));
#else
    const int savedOut = dup(fileno(stdout));
    const int savedErr = dup(fileno(stderr));
    dup2(fileno(outFile), fileno(stdout));
    dup2(fileno(errFile), fileno(stderr));
#endif
    body();
    std::fflush(stdout);
    std::fflush(stderr);
#if defined(_WIN32)
    _dup2(savedOut, _fileno(stdout));
    _dup2(savedErr, _fileno(stderr));
    _close(savedOut);
    _close(savedErr);
#else
    dup2(savedOut, fileno(stdout));
    dup2(savedErr, fileno(stderr));
    close(savedOut);
    close(savedErr);
#endif
    Streams streams{ReadAll(outFile), ReadAll(errFile)};
    std::fclose(outFile);
    std::fclose(errFile);
    return streams;
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

std::string ExecutableName() {
#if defined(_WIN32)
    HMODULE module = nullptr;
    Require(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&ExecutableName), &module) != 0 && module != nullptr,
            "cannot find the test module");
    std::string name = BacktraceModulePath_nid_no_patch(module, 1);
#else
    Dl_info info{};
    Require(dladdr(reinterpret_cast<void*>(&ExecutableName), &info) != 0 && info.dli_fname != nullptr, "cannot find the test module");
    std::string name = info.dli_fname;
#endif
    if (const auto slash = name.find_last_of("/\\"); slash != std::string::npos) name.erase(0, slash + 1);
    return name;
}

[[gnu::noinline]] void CallPrint() {
    Require(sceKernelPrintBacktraceWithModuleInfo() == 0, "call failed");
    Require(sceKernelPrintBacktraceWithModuleInfo() == 0, "second call failed");
}

}

int main() {
    const std::string executable = ExecutableName();
    const auto streams = Capture(&CallPrint);
    Require(streams.out.empty(), "wrote to stdout");
    const auto lines = Lines(streams.err);
    std::vector<std::size_t> headers;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (lines[index] == "backtrace:") headers.push_back(index);
    }
    Require(headers.size() == 2, "wrong number of headers");
    Require(headers.front() == 0, "header is not first");
    Require(streams.err.find("not implemented") == std::string::npos, "reported a missing implementation");
    for (std::size_t block = 0; block < headers.size(); ++block) {
        const std::size_t begin = headers[block];
        const std::size_t end = block + 1 < headers.size() ? headers[block + 1] : lines.size();
        Require(end > begin + 2, "block has too few lines");
        Require(lines[end - 1].find("0x0 =>") != std::string::npos, "block does not end with a zero address");
        for (std::size_t index = begin + 1; index < end; ++index) {
            const auto& line = lines[index];
            Require(!line.empty() && line.front() == '[' && line.find("    0x") != std::string::npos
                        && line.find(" => ") != std::string::npos && line.rfind("+0x") != std::string::npos,
                    "frame line does not match the measured shape");
        }
    }
    Require(lines[headers.front() + 1].find(executable) != std::string::npos, "first frame is not in the test module");
    return 0;
}

#include "prx/libSceAddressSanitizer/src/ReportFile.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/ModuleLookup.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace AsanRuntime::ReportFile {

namespace {

constexpr std::size_t kMaxPathLength = 4096;
constexpr int kStdoutDescriptor = 1;
constexpr int kStderrDescriptor = 2;

enum class Mode {
    Standard,
    Descriptor,
    Path,
};

struct State {
    std::mutex mutex;
    Mode mode = Mode::Standard;
    int descriptor = kStderrDescriptor;
    std::string prefix;
    std::string fullPath;
    FileStream* file = nullptr;
    std::uintptr_t filePid = 0;
};

State& GetState() {
    static State state;
    return state;
}

void WriteStandard(int descriptor, const std::string& text) {
    std::FILE* stream = descriptor == kStdoutDescriptor ? stdout : stderr;
    std::fputs(text.c_str(), stream);
    std::fflush(stream);
}

void ReopenIfNecessary(State& state) {
    if (state.mode != Mode::Path) {
        return;
    }
    const std::uintptr_t pid = HostProcessId_nid_no_patch();
    if (state.file) {
        if (state.filePid == pid) {
            return;
        }
        fclose_nid_postfix(state.file);
        state.file = nullptr;
    }
    state.fullPath = state.prefix + "." + std::to_string(pid);
    errno = 0;
    state.file = fopen_nid_postfix(state.fullPath.c_str(), "ab");
    if (!state.file) {
        const std::string message = "ERROR: Can't open file: " + state.fullPath + " (reason: " + std::to_string(errno) + ")";
        WriteStandard(kStderrDescriptor, message);
        Die_nid_no_patch();
    }
    state.filePid = pid;
}

void CloseOwnedFile(State& state) {
    if (state.file) {
        fclose_nid_postfix(state.file);
        state.file = nullptr;
    }
}

}  // namespace

void SetPath_nid_no_patch(const char* path) {
    if (path && std::strlen(path) > kMaxPathLength - 100) {
        char prefix[9] = {};
        std::memcpy(prefix, path, 8);
        WriteStandard(kStderrDescriptor, std::string("ERROR: Path is too long: ") + prefix + "...\n");
        Die_nid_no_patch();
    }
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    CloseOwnedFile(state);
    if (!path || std::strcmp(path, "stderr") == 0) {
        state.mode = Mode::Standard;
        state.descriptor = kStderrDescriptor;
    } else if (std::strcmp(path, "stdout") == 0) {
        state.mode = Mode::Standard;
        state.descriptor = kStdoutDescriptor;
    } else {
        state.mode = Mode::Path;
        state.prefix = path;
    }
}

void SetDescriptor_nid_no_patch(int descriptor) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    CloseOwnedFile(state);
    if (descriptor == kStdoutDescriptor || descriptor == kStderrDescriptor) {
        state.mode = Mode::Standard;
    } else {
        state.mode = Mode::Descriptor;
    }
    state.descriptor = descriptor;
}

const char* GetPath_nid_no_patch() {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    ReopenIfNecessary(state);
    return state.fullPath.c_str();
}

void Write_nid_no_patch(const std::string& text) {
    State& state = GetState();
    std::lock_guard lock(state.mutex);
    ReopenIfNecessary(state);
    switch (state.mode) {
    case Mode::Standard:
        WriteStandard(state.descriptor, text);
        break;
    case Mode::Descriptor:
        dprintf_nid_postfix(state.descriptor, "%s", text.c_str());
        break;
    case Mode::Path:
        fwrite_nid_postfix(text.data(), 1, text.size(), state.file);
        fflush_nid_postfix(state.file);
        break;
    }
}

}  // namespace AsanRuntime::ReportFile

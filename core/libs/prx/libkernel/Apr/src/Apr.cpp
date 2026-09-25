#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr int GUEST_ENOENT = 2;
constexpr int GUEST_EINVAL = 22;

struct AprFile {
    std::filesystem::path path;
    std::uint64_t size;
};

std::mutex g_filesLock;
std::vector<AprFile> g_files;
std::unordered_map<std::string, std::uint32_t> g_idsByPath;

// File sizes by directory, listed on first use. Titles resolve thousands of package paths at startup,
// and one size query per path took over 20 s; a listing costs one directory read. Names are compared
// case-insensitively, like the host file system.
std::mutex g_directoriesLock;
std::unordered_map<std::string, std::unordered_map<std::string, std::uint64_t>> g_directories;

std::string _foldCase(std::string text) {
    for (auto& character : text) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return text;
}

bool _fileSize(const std::filesystem::path& path, std::uint64_t& bytes) {
    {
        std::lock_guard lock(g_directoriesLock);
        const auto directory = _foldCase(path.parent_path().string());
        auto listed = g_directories.find(directory);
        if (listed == g_directories.end()) {
            std::unordered_map<std::string, std::uint64_t> sizes;
#ifdef _WIN32
            // The find data carries each size; std::filesystem would query every entry again.
            WIN32_FIND_DATAW entry{};
            const HANDLE find = FindFirstFileExW((path.parent_path() / L"*").c_str(), FindExInfoBasic, &entry, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (find != INVALID_HANDLE_VALUE) {
                do {
                    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
                    const auto size = (static_cast<std::uint64_t>(entry.nFileSizeHigh) << 32u) | entry.nFileSizeLow;
                    sizes.emplace(_foldCase(std::filesystem::path(entry.cFileName).string()), size);
                } while (FindNextFileW(find, &entry));
                FindClose(find);
            }
#else
            std::error_code error;
            for (std::filesystem::directory_iterator it(path.parent_path(), error), end; !error && it != end; it.increment(error)) {
                std::error_code entryError;
                if (!it->is_regular_file(entryError)) continue;
                const auto size = it->file_size(entryError);
                if (!entryError) sizes.emplace(_foldCase(it->path().filename().string()), size);
            }
#endif
            listed = g_directories.emplace(directory, std::move(sizes)).first;
        }
        if (const auto file = listed->second.find(_foldCase(path.filename().string())); file != listed->second.end()) {
            bytes = file->second;
            return true;
        }
    }
    // Not in the listing (created since, or an unusual name): ask the file system directly.
    std::error_code error;
    bytes = std::filesystem::file_size(path, error);
    return !error;
}

int _fail(int guestErrno) {
    errno = guestErrno;
    return -1;
}

bool _resolve(const char* guestPath, std::uint32_t* id, std::uint64_t* size) {
    if (!guestPath) return false;
    auto hostPath = ResolvePath_nid_no_patch(guestPath);
    std::uint64_t bytes = 0;
    if (!_fileSize(hostPath, bytes)) return false;
    std::lock_guard lock(g_filesLock);
    const auto key = hostPath.string();
    auto found = g_idsByPath.find(key);
    if (found == g_idsByPath.end()) {
        found = g_idsByPath.emplace(key, static_cast<std::uint32_t>(g_files.size())).first;
        g_files.push_back({std::move(hostPath), bytes});
    }
    if (id) *id = found->second;
    if (size) *size = bytes;
    return true;
}

AprFile _file(std::uint32_t id) {
    std::lock_guard lock(g_filesLock);
    if (id >= g_files.size()) throw std::runtime_error("APR: unknown file id " + std::to_string(id));
    return g_files[id];
}

void _readFile(const Apr::ReadFileCommand& command) {
    const auto file = _file(command.fileId);
    static const bool trace = std::getenv("APS5_TRACE_APR") != nullptr;
    if (trace) std::fprintf(stderr, "[apr] read %s offset=0x%llx size=0x%llx -> 0x%llx\n", file.path.string().c_str(), static_cast<unsigned long long>(command.offset), static_cast<unsigned long long>(command.size), static_cast<unsigned long long>(command.destination));
    std::ifstream stream(file.path, std::ios::binary);
    if (!stream) throw std::runtime_error("APR: cannot open " + file.path.string());
    stream.seekg(static_cast<std::streamoff>(command.offset));
    stream.read(reinterpret_cast<char*>(command.destination), static_cast<std::streamsize>(command.size));
    if (stream.bad()) throw std::runtime_error("APR: read failed for " + file.path.string());
}

void _writeAddress(const Apr::WriteAddressCommand& command) {
    if (command.flags != 0) throw std::runtime_error("APR: WriteAddress flags " + std::to_string(command.flags) + " not implemented");
    std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).store(command.value, std::memory_order_release);
}

void _execute(const Apr::CommandBufferObject& buffer) {
    std::uint32_t cursor = 0;
    for (std::uint32_t index = 0; index < buffer.numCommands; ++index) {
        if (cursor + sizeof(Apr::CommandHeader) > buffer.offset) throw std::runtime_error("APR: truncated command buffer");
        Apr::CommandHeader header;
        std::memcpy(&header, buffer.base + cursor, sizeof(header));
        if (header.bytes < sizeof(header) || cursor + header.bytes > buffer.offset) throw std::runtime_error("APR: malformed command");
        switch (header.opcode) {
        case Apr::Opcode::Nop:
            break;
        case Apr::Opcode::ReadFile: {
            Apr::ReadFileCommand command;
            std::memcpy(&command, buffer.base + cursor, sizeof(command));
            _readFile(command);
            break;
        }
        case Apr::Opcode::WriteAddress: {
            Apr::WriteAddressCommand command;
            std::memcpy(&command, buffer.base + cursor, sizeof(command));
            _writeAddress(command);
            break;
        }
        default:
            throw std::runtime_error("APR: unknown opcode " + std::to_string(static_cast<std::uint32_t>(header.opcode)));
        }
        cursor += header.bytes;
    }
}

}

extern "C" {

int APS5_VABI sceKernelAprResolveFilepathsToIds(const char** paths, uint32_t count, uint32_t* ids, uint32_t* error_index) {
    if (!paths || !ids) return _fail(GUEST_EINVAL);
    for (uint32_t index = 0; index < count; ++index) {
        if (!_resolve(paths[index], &ids[index], nullptr)) {
            if (error_index) *error_index = index;
            return _fail(GUEST_ENOENT);
        }
    }
    return 0;
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* error_index) {
    if (!paths || !ids || !sizes) return _fail(GUEST_EINVAL);
    for (uint32_t index = 0; index < count; ++index) {
        if (!_resolve(paths[index], &ids[index], &sizes[index])) {
            if (error_index) *error_index = index;
            return _fail(GUEST_ENOENT);
        }
    }
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBuffer(const Apr::CommandBufferObject* buffer, uint32_t priority) {
    (void)priority;
    if (!buffer || buffer->type != Apr::BufferType::Apr) return _fail(GUEST_EINVAL);
    _execute(*buffer);
    return 0;
}

}

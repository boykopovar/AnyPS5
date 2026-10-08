#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
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

constexpr std::uint32_t INVALID_FILE_ID = 0xFFFFFFFFu;
constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002);

int _resolveForEach(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    if (!paths || !ids) return _fail(GUEST_EINVAL);
    for (uint32_t index = 0; index < count; ++index) {
        const std::string path = prefix ? (paths[index] ? std::string(prefix) + paths[index] : std::string()) : (paths[index] ? std::string(paths[index]) : std::string());
        const bool resolved = !path.empty() && _resolve(path.c_str(), &ids[index], sizes ? &sizes[index] : nullptr);
        if (!resolved) {
            ids[index] = INVALID_FILE_ID;
            if (sizes) sizes[index] = 0;
        }
        if (results) results[index] = resolved ? 0 : SCE_KERNEL_ERROR_ENOENT;
    }
    return 0;
}

void _readFile(const Apr::ReadFileCommand& command) {
    const auto file = _file(command.fileId);
    static const bool trace = std::getenv("APS5_TRACE_APR") != nullptr;
    if (trace) std::fprintf(stderr, "[apr] read %s offset=0x%llx size=0x%llx -> 0x%llx\n", file.path.string().c_str(), static_cast<unsigned long long>(command.offset), static_cast<unsigned long long>(command.size), static_cast<unsigned long long>(command.destination));
    std::ifstream stream(file.path, std::ios::binary);
    if (!stream) throw std::runtime_error("APR: cannot open " + file.path.string());
    stream.seekg(static_cast<std::streamoff>(command.offset));
    const GuestArena::HostWrite destination(reinterpret_cast<void*>(command.destination), command.size);
    if (!destination.Open()) throw std::runtime_error("APR: the read destination of " + file.path.string() + " is not writable guest memory");
    stream.read(reinterpret_cast<char*>(command.destination), static_cast<std::streamsize>(command.size));
    if (stream.bad()) throw std::runtime_error("APR: read failed for " + file.path.string());
    const auto read = static_cast<std::uint64_t>(stream.gcount());
    if (read != command.size) throw std::runtime_error("APR: read of " + file.path.string() + " at offset " + std::to_string(command.offset) + " returned " + std::to_string(read) + " of " + std::to_string(command.size) + " bytes");
}

void _writeAddress(const Apr::WriteAddressCommand& command) {
    if (command.flags != 0) throw std::runtime_error("APR: WriteAddress flags " + std::to_string(command.flags) + " not implemented");
    std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).store(command.value, std::memory_order_release);
}

std::array<std::atomic<std::uint32_t>, 256> g_counters{};

std::mutex g_counterWrites;

std::uint32_t _counter(std::uint32_t index) {
    return g_counters[index % g_counters.size()].load(std::memory_order_acquire);
}

struct CounterField {
    std::uint32_t bits;
    std::uint32_t shift;
};

CounterField _counterField(Apr::CounterAccess access) {
    const auto value = static_cast<std::uint32_t>(access);
    if (value == 0) return {64, 0};
    if (value == 1) return {32, 0};
    if (value < 4) return {16, (value - 2) * 16};
    if (value < 8) return {8, (value - 4) * 8};
    throw std::runtime_error("APR: counter access " + std::to_string(value) + " not implemented");
}

std::uint64_t _fieldMask(std::uint32_t bits) {
    return bits == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1u;
}

std::uint64_t _readCounter(std::uint32_t index, Apr::CounterAccess access) {
    const auto field = _counterField(access);
    if (field.bits == 64) return _counter(index) | static_cast<std::uint64_t>(_counter(index + 1u)) << 32u;
    return (_counter(index) >> field.shift) & _fieldMask(field.bits);
}

std::uint64_t _applyCounterOperation(Apr::CounterOperation operation, std::uint64_t current, std::uint64_t value) {
    switch (operation) {
        case Apr::CounterOperation::Store: return value;
        case Apr::CounterOperation::AtomicOr: return current | value;
        case Apr::CounterOperation::AtomicAndComplement: return current & ~value;
        case Apr::CounterOperation::AtomicXor: return current ^ value;
        case Apr::CounterOperation::AtomicAdd: return current + value;
    }
    throw std::runtime_error("APR: counter operation " + std::to_string(static_cast<std::uint32_t>(operation)) + " not implemented");
}

void _writeCounter(const Apr::WriteCounterCommand& command) {
    const auto field = _counterField(command.access);
    const std::lock_guard lock(g_counterWrites);
    const std::uint64_t current = _readCounter(command.counter, command.access);
    const std::uint64_t next = _applyCounterOperation(command.operation, current, command.value) & _fieldMask(field.bits);
    auto& low = g_counters[command.counter % g_counters.size()];
    if (field.bits == 64) {
        g_counters[(command.counter + 1u) % g_counters.size()].store(static_cast<std::uint32_t>(next >> 32u), std::memory_order_release);
        low.store(static_cast<std::uint32_t>(next), std::memory_order_release);
        return;
    }
    const auto mask = static_cast<std::uint32_t>(_fieldMask(field.bits) << field.shift);
    const auto bits = static_cast<std::uint32_t>(next << field.shift);
    low.store((low.load(std::memory_order_relaxed) & ~mask) | bits, std::memory_order_release);
}

bool _waitSatisfied(std::uint32_t compare, std::uint64_t value, std::uint64_t reference) {
    constexpr std::uint64_t sign = std::uint64_t{1} << 63u;
    switch (compare) {
        case 0: return value == reference;
        case 1: return value > reference;
        case 2: return value < reference;
        case 3: return value != reference;
        case 4: return value - reference < sign;
        case 5: return (value ^ sign) > (reference ^ sign);
        case 6: return (value ^ sign) < (reference ^ sign);
        default: throw std::runtime_error("APR: wait compare function " + std::to_string(compare) + " not implemented");
    }
}

template<class TCommand>
TCommand _read(const Apr::CommandBufferObject& buffer, std::uint32_t cursor) {
    TCommand command;
    std::memcpy(&command, buffer.base + cursor, sizeof(command));
    return command;
}

constexpr std::uint64_t AmmRangeBytes = 32ull << 30;
constexpr int GuestMapFixed = 0x10;

struct AmmPage {
    std::uint64_t physical;
    std::int32_t type;
    std::int32_t protection;
    bool pooled;
};

struct AmmState {
    std::mutex lock;
    std::uintptr_t base = 0;
    std::map<std::uint64_t, std::uint64_t> pool;
    std::map<std::uintptr_t, AmmPage> pages;
    std::map<std::uint64_t, std::uint32_t> pooledUses;
    std::set<std::uintptr_t> prt;
    std::atomic<std::uint32_t> lastSubmit{0};
};

AmmState& _amm() {
    static AmmState state;
    return state;
}

std::uintptr_t _ammBase(AmmState& state) {
    if (state.base == 0) {
        void* address = nullptr;
        if (DoReserveVirtual(&address, 2 * AmmRangeBytes, 0, 0x200000) != 0) throw std::runtime_error("AMM: cannot reserve the virtual address range");
        state.base = reinterpret_cast<std::uintptr_t>(address);
    }
    return state.base;
}

void _ammCheckRange(AmmState& state, std::uint64_t address, std::uint64_t size) {
    const auto base = _ammBase(state);
    if (address < base || size > 2 * AmmRangeBytes || address - base > 2 * AmmRangeBytes - size) throw std::runtime_error("AMM: range outside the AMM virtual address range");
}

int _ammHostProtection(std::int32_t protection) {
    int host = protection & 0x33;
    if ((protection & 0x140) != 0) host |= 1;
    if ((protection & 0x280) != 0) host |= 3;
    if ((host & 2) != 0) host |= 1;
    return host;
}

void _returnPoolPages(AmmState& state, std::uint64_t offset, std::uint64_t bytes) {
    auto next = state.pool.lower_bound(offset);
    if (next != state.pool.end() && next->first == offset + bytes) {
        bytes += next->second;
        next = state.pool.erase(next);
    }
    if (next != state.pool.begin()) {
        const auto previous = std::prev(next);
        if (previous->first + previous->second == offset) {
            previous->second += bytes;
            return;
        }
    }
    state.pool.emplace(offset, bytes);
}

std::uint64_t _takePoolPages(AmmState& state, std::uint64_t bytes) {
    for (auto it = state.pool.begin(); it != state.pool.end(); ++it) {
        if (it->second < bytes) continue;
        const auto offset = it->first;
        const auto remaining = it->second - bytes;
        state.pool.erase(it);
        if (remaining != 0) state.pool.emplace(offset + bytes, remaining);
        return offset;
    }
    throw std::runtime_error("AMM: no free run of " + std::to_string(bytes) + " bytes in the direct memory given to the mapper");
}

void _addPage(AmmState& state, std::uint64_t address, const AmmPage& page) {
    state.pages[address] = page;
    if (page.pooled) ++state.pooledUses[page.physical];
}

void _forgetPages(AmmState& state, std::uint64_t address, std::uint64_t size, bool release) {
    for (auto it = state.pages.lower_bound(address); it != state.pages.end() && it->first < address + size;) {
        if (it->second.pooled) {
            const auto uses = state.pooledUses.find(it->second.physical);
            if (--uses->second == 0 && release) {
                state.pooledUses.erase(uses);
                _returnPoolPages(state, it->second.physical, PS5_PAGE_SIZE);
            }
        }
        it = state.pages.erase(it);
    }
}

std::vector<AmmPage> _mappedPages(AmmState& state, std::uint64_t address, std::uint64_t size) {
    std::vector<AmmPage> pages;
    for (std::uint64_t offset = 0; offset < size; offset += PS5_PAGE_SIZE) {
        const auto found = state.pages.find(address + offset);
        if (found == state.pages.end()) throw std::runtime_error("AMM: range is not fully mapped");
        pages.push_back(found->second);
    }
    return pages;
}

void _reserve(std::uint64_t address, std::uint64_t size) {
    void* target = reinterpret_cast<void*>(address);
    if (DoReserveVirtual(&target, size, GuestMapFixed, 0) != 0) throw std::runtime_error("AMM: cannot return the range to the reservation");
}

void _mapRun(std::uint64_t address, std::uint64_t physical, std::uint64_t size, std::int32_t protection) {
    void* target = reinterpret_cast<void*>(address);
    if (DoMapDirect(&target, size, _ammHostProtection(protection), GuestMapFixed, static_cast<std::int64_t>(physical), 0) != 0) throw std::runtime_error("AMM: map failed");
}

void _mapPages(AmmState& state, std::uint64_t address, const std::vector<AmmPage>& pages) {
    for (std::size_t first = 0; first < pages.size();) {
        std::size_t last = first + 1;
        while (last < pages.size() && pages[last].physical == pages[last - 1].physical + PS5_PAGE_SIZE && pages[last].protection == pages[first].protection) ++last;
        _mapRun(address + first * PS5_PAGE_SIZE, pages[first].physical, (last - first) * PS5_PAGE_SIZE, pages[first].protection);
        first = last;
    }
    for (std::size_t index = 0; index < pages.size(); ++index) _addPage(state, address + index * PS5_PAGE_SIZE, pages[index]);
}

void _clearPrt(AmmState& state, std::uint64_t address, std::uint64_t size) {
    state.prt.erase(state.prt.lower_bound(address), state.prt.lower_bound(address + size));
}

void _requirePrt(AmmState& state, std::uint64_t address, std::uint64_t size) {
    for (std::uint64_t offset = 0; offset < size; offset += PS5_PAGE_SIZE) {
        if (!state.prt.contains(address + offset)) throw std::runtime_error("AMM: range is not a PRT range");
    }
}

void _zeroPrt(std::uint64_t address, std::uint64_t size) {
    constexpr int GpuRead = 0x10;
    void* target = reinterpret_cast<void*>(address);
    if (DoMapAnon(&target, size, GpuRead, GuestMapFixed) != 0) throw std::runtime_error("AMM: cannot map the unbacked PRT pages");
}

void _ammUnmap(const Apr::AmmUnmapCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _forgetPages(state, command.address, command.size, true);
    _clearPrt(state, command.address, command.size);
    _reserve(command.address, command.size);
}

void _ammMapAsPrt(const Apr::AmmUnmapCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _forgetPages(state, command.address, command.size, true);
    _zeroPrt(command.address, command.size);
    for (std::uint64_t offset = 0; offset < command.size; offset += PS5_PAGE_SIZE) state.prt.insert(command.address + offset);
}

void _ammUnmapToPrt(const Apr::AmmUnmapCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _requirePrt(state, command.address, command.size);
    _forgetPages(state, command.address, command.size, true);
    _zeroPrt(command.address, command.size);
}

void _ammAllocatePrt(const Apr::AmmProtectCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _requirePrt(state, command.address, command.size);
    for (std::uint64_t offset = 0; offset < command.size;) {
        const auto address = command.address + offset;
        const bool backed = state.pages.contains(address);
        std::uint64_t end = offset + PS5_PAGE_SIZE;
        while (end < command.size && state.pages.contains(command.address + end) == backed) end += PS5_PAGE_SIZE;
        const auto bytes = end - offset;
        if (backed) {
            for (std::uint64_t page = 0; page < bytes; page += PS5_PAGE_SIZE) {
                auto& entry = state.pages[address + page];
                entry.type = command.type;
                entry.protection = command.protection;
            }
            if (DoMprotect(reinterpret_cast<void*>(address), bytes, _ammHostProtection(command.protection)) != 0) throw std::runtime_error("AMM: protection change failed");
        } else {
            std::vector<AmmPage> pages;
            try {
                for (std::uint64_t page = 0; page < bytes; page += PS5_PAGE_SIZE) pages.push_back({_takePoolPages(state, PS5_PAGE_SIZE), command.type, command.protection, true});
            } catch (...) {
                for (const auto& page : pages) _returnPoolPages(state, page.physical, PS5_PAGE_SIZE);
                throw;
            }
            _mapPages(state, address, pages);
        }
        offset = end;
    }
}

void _ammMap(const Apr::AmmMapCommand& command, bool direct) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _forgetPages(state, command.address, command.size, true);
    _clearPrt(state, command.address, command.size);
    const auto physical = direct ? command.directOffset : _takePoolPages(state, command.size);
    try {
        _mapRun(command.address, physical, command.size, command.protection);
    } catch (...) {
        if (!direct) _returnPoolPages(state, physical, command.size);
        throw;
    }
    for (std::uint64_t offset = 0; offset < command.size; offset += PS5_PAGE_SIZE) _addPage(state, command.address + offset, {physical + offset, command.type, command.protection, !direct});
}

void _ammRemap(const Apr::AmmRemapCommand& command, bool alias, bool intoPrt) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _ammCheckRange(state, command.source, command.size);
    if ((alias || intoPrt) && command.address < command.source + command.size && command.source < command.address + command.size) throw std::runtime_error("AMM: remap onto its own source range");
    if (intoPrt) _requirePrt(state, command.address, command.size);
    auto pages = _mappedPages(state, command.source, command.size);
    for (auto& page : pages) page.protection = command.protection;
    if (!alias) {
        _forgetPages(state, command.source, command.size, false);
        _clearPrt(state, command.source, command.size);
        _reserve(command.source, command.size);
    }
    _forgetPages(state, command.address, command.size, true);
    if (!intoPrt) _clearPrt(state, command.address, command.size);
    _mapPages(state, command.address, pages);
}

void _ammProtect(const Apr::AmmProtectCommand& command, bool type) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    auto pages = _mappedPages(state, command.address, command.size);
    for (std::size_t index = 0; index < pages.size(); ++index) {
        auto& page = pages[index];
        page.protection = (page.protection & ~command.mask) | (command.protection & command.mask);
        if (type) page.type = command.type;
        state.pages[command.address + index * PS5_PAGE_SIZE] = page;
    }
    for (std::size_t first = 0; first < pages.size();) {
        const int host = _ammHostProtection(pages[first].protection);
        std::size_t last = first + 1;
        while (last < pages.size() && _ammHostProtection(pages[last].protection) == host) ++last;
        if (DoMprotect(reinterpret_cast<void*>(command.address + first * PS5_PAGE_SIZE), (last - first) * PS5_PAGE_SIZE, host) != 0) throw std::runtime_error("AMM: protection change failed");
        first = last;
    }
}

struct ReadCursor {
    bool valid = false;
    std::uint32_t fileId = 0;
    std::uint64_t nextDestination = 0;
    std::uint64_t nextOffset = 0;
};

void _readResolved(Apr::Opcode opcode, Apr::ReadFileCommand command, ReadCursor& read) {
    if (opcode != Apr::Opcode::ReadFile) {
        if (!read.valid) throw std::runtime_error("APR: gather or scatter read without a preceding read");
        command.fileId = read.fileId;
        if (opcode == Apr::Opcode::ReadFileGather) command.destination = read.nextDestination;
        if (opcode == Apr::Opcode::ReadFileScatter) command.offset = read.nextOffset;
    }
    _readFile(command);
    read = {true, command.fileId, command.destination + command.size, command.offset + command.size};
}

void _execute(const Apr::CommandBufferObject& buffer) {
    std::uint32_t cursor = 0;
    ReadCursor read;
    while (cursor < buffer.offset) {
        if (cursor + sizeof(Apr::CommandHeader) > buffer.offset) throw std::runtime_error("APR: truncated command buffer");
        Apr::CommandHeader header;
        std::memcpy(&header, buffer.base + cursor, sizeof(header));
        if (header.bytes < sizeof(header) || cursor + header.bytes > buffer.offset) throw std::runtime_error("APR: malformed command");
        switch (header.opcode) {
        case Apr::Opcode::Nop:
        case Apr::Opcode::PushMarker:
        case Apr::Opcode::PopMarker:
        case Apr::Opcode::SetMarker:
            break;
        case Apr::Opcode::ReadFile:
        case Apr::Opcode::ReadFileGather:
        case Apr::Opcode::ReadFileScatter:
        case Apr::Opcode::ReadFileGatherScatter:
            _readResolved(header.opcode, _read<Apr::ReadFileCommand>(buffer, cursor), read);
            break;
        case Apr::Opcode::ResetGatherScatterState:
            read = {};
            break;
        case Apr::Opcode::AmmMap:
        case Apr::Opcode::AmmMapDirect:
            _ammMap(_read<Apr::AmmMapCommand>(buffer, cursor), header.opcode == Apr::Opcode::AmmMapDirect);
            break;
        case Apr::Opcode::AmmUnmap:
            _ammUnmap(_read<Apr::AmmUnmapCommand>(buffer, cursor));
            break;
        case Apr::Opcode::AmmRemap:
        case Apr::Opcode::AmmMultiMap:
            _ammRemap(_read<Apr::AmmRemapCommand>(buffer, cursor), header.opcode == Apr::Opcode::AmmMultiMap, false);
            break;
        case Apr::Opcode::AmmRemapIntoPrt:
            _ammRemap(_read<Apr::AmmRemapCommand>(buffer, cursor), false, true);
            break;
        case Apr::Opcode::AmmMapAsPrt:
            _ammMapAsPrt(_read<Apr::AmmUnmapCommand>(buffer, cursor));
            break;
        case Apr::Opcode::AmmUnmapToPrt:
            _ammUnmapToPrt(_read<Apr::AmmUnmapCommand>(buffer, cursor));
            break;
        case Apr::Opcode::AmmAllocatePaForPrt:
            _ammAllocatePrt(_read<Apr::AmmProtectCommand>(buffer, cursor));
            break;
        case Apr::Opcode::AmmModifyProtect:
        case Apr::Opcode::AmmModifyMtypeProtect:
            _ammProtect(_read<Apr::AmmProtectCommand>(buffer, cursor), header.opcode == Apr::Opcode::AmmModifyMtypeProtect);
            break;
        case Apr::Opcode::MapEnd:
            break;
        case Apr::Opcode::WriteAddress: {
            Apr::WriteAddressCommand command;
            std::memcpy(&command, buffer.base + cursor, sizeof(command));
            _writeAddress(command);
            break;
        }
        case Apr::Opcode::WriteCounter: {
            _writeCounter(_read<Apr::WriteCounterCommand>(buffer, cursor));
            break;
        }
        case Apr::Opcode::WaitOnAddress:
        case Apr::Opcode::WaitOnCounter: {
            const auto command = _read<Apr::WaitCommand>(buffer, cursor);
            const bool counter = header.opcode == Apr::Opcode::WaitOnCounter;
            const std::uint32_t unused = counter ? 64u - _counterField(command.access).bits : 0u;
            const auto current = [&]() -> std::uint64_t {
                if (counter) return _readCounter(command.counter, command.access);
                return std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).load(std::memory_order_acquire);
            };
            const std::uint64_t reference = (command.reference & command.mask) << unused;
            while (!_waitSatisfied(command.compare, (current() & command.mask) << unused, reference)) std::this_thread::sleep_for(std::chrono::microseconds(50));
            break;
        }
        case Apr::Opcode::WriteKernelEventQueue: {
            const auto command = _read<Apr::WriteKernelEventQueueCommand>(buffer, cursor);
            EqueueTriggerEvent_nid_postfix(static_cast<KernelEqueue>(command.equeue), static_cast<uintptr_t>(command.ident), EVFILT_AMPR, reinterpret_cast<void*>(command.data));
            break;
        }
        case Apr::Opcode::WriteAddressFromTimeCounter: {
            const auto command = _read<Apr::WriteAddressFromCounterCommand>(buffer, cursor);
            std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).store(sceKernelGetProcessTimeCounter(), std::memory_order_release);
            break;
        }
        case Apr::Opcode::WriteAddressFromCounter:
        case Apr::Opcode::WriteAddressFromCounterPair: {
            const auto command = _read<Apr::WriteAddressFromCounterCommand>(buffer, cursor);
            std::uint64_t value = _counter(command.counter0);
            if (header.opcode == Apr::Opcode::WriteAddressFromCounterPair) value |= static_cast<std::uint64_t>(_counter(command.counter1)) << 32u;
            std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).store(value, std::memory_order_release);
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

int APS5_VABI sceKernelAllocateDirectMemory(int64_t searchStart, int64_t searchEnd, size_t length, size_t alignment, int memoryType, int64_t* offset);

int AmmGiveDirectMemory_nid_no_patch(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    if (!offset || (usage != 0 && usage != 1)) return SCE_KERNEL_ERROR_EINVAL;
    const int result = sceKernelAllocateDirectMemory(searchStart, searchEnd, size, alignment, 0, offset);
    if (result != 0 || usage == 0) return result;
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _returnPoolPages(state, static_cast<std::uint64_t>(*offset), size);
    return 0;
}

void AmmVirtualAddressRanges_nid_no_patch(std::uint64_t* start, std::uint64_t* end, std::uint64_t* multimapStart, std::uint64_t* multimapEnd) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    const auto base = _ammBase(state);
    if (start) *start = base;
    if (end) *end = base + AmmRangeBytes;
    if (multimapStart) *multimapStart = base + AmmRangeBytes;
    if (multimapEnd) *multimapEnd = base + 2 * AmmRangeBytes;
}

std::uint32_t AmmSubmit_nid_no_patch(void* base, std::uint32_t bytes) {
    _execute(Apr::CommandBufferObject{static_cast<std::uint8_t*>(base), bytes, bytes, 0, Apr::BufferType::Generic, 0});
    auto& last = _amm().lastSubmit;
    std::uint32_t id = last.fetch_add(1) + 1;
    while (id == 0) id = last.fetch_add(1) + 1;
    return id;
}

bool AmmSubmitted_nid_no_patch(std::uint32_t id) {
    return id != 0 && id <= _amm().lastSubmit.load();
}


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

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* error_index) {
    if (!prefix || !paths || !ids) return _fail(GUEST_EINVAL);
    for (uint32_t index = 0; index < count; ++index) {
        const std::string path = paths[index] ? std::string(prefix) + paths[index] : std::string();
        if (path.empty() || !_resolve(path.c_str(), &ids[index], sizes ? &sizes[index] : nullptr)) {
            if (error_index) *error_index = index;
            return _fail(GUEST_ENOENT);
        }
    }
    return 0;
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIds(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint32_t* error_index) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes(prefix, paths, count, ids, nullptr, error_index);
}

int APS5_VABI sceKernelAprGetFileSize(uint32_t id, uint64_t* size) {
    if (!size) return _fail(GUEST_EINVAL);
    *size = _file(id).size;
    return 0;
}

int APS5_VABI sceKernelAprGetFileStat(uint32_t id, FileStat* stat) {
    if (!stat) return _fail(GUEST_EINVAL);
    File::FillFileStat(_file(id).path, stat);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBuffer(const Apr::CommandBufferObject* buffer, uint32_t priority) {
    (void)priority;
    if (!buffer || buffer->type != Apr::BufferType::Apr) return _fail(GUEST_EINVAL);
    _execute(*buffer);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetId(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* id) {
    if (!id) return _fail(GUEST_EINVAL);
    const int result = sceKernelAprSubmitCommandBuffer(buffer, priority);
    if (result != 0) return result;
    static std::atomic<uint32_t> nextId{1};
    *id = nextId.fetch_add(1);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* result, uint32_t* id) {
    if (!result) return _fail(GUEST_EINVAL);
    const int submitted = sceKernelAprSubmitCommandBufferAndGetId(buffer, priority, id);
    if (submitted != 0) return submitted;
    *result = 0;
    return 0;
}

int APS5_VABI sceKernelAprWaitCommandBuffer(uint32_t id) {
    (void)id;
    return 0;
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsForEach(const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return _resolveForEach(nullptr, paths, count, ids, nullptr, results);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizesForEach(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    if (!sizes) return _fail(GUEST_EINVAL);
    return _resolveForEach(nullptr, paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsForEach(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, int* results) {
    if (!prefix) return _fail(GUEST_EINVAL);
    return _resolveForEach(prefix, paths, count, ids, nullptr, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    if (!prefix || !sizes) return _fail(GUEST_EINVAL);
    return _resolveForEach(prefix, paths, count, ids, sizes, results);
}

}

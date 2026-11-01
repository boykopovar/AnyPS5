#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <optional>
#include <deque>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libkernel/Apr/include/AmmPool.hpp"
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

std::mutex& g_filesLock = *new std::mutex();
std::vector<AprFile>& g_files = *new std::vector<AprFile>();
std::unordered_map<std::string, std::uint32_t>& g_idsByPath = *new std::unordered_map<std::string, std::uint32_t>();

// File sizes by directory, listed on first use. Titles resolve thousands of package paths at startup,
// and one size query per path took over 20 s; a listing costs one directory read. Names are compared
// case-insensitively, like the host file system.
std::mutex& g_directoriesLock = *new std::mutex();
std::unordered_map<std::string, std::unordered_map<std::string, std::uint64_t>>& g_directories = *new std::unordered_map<std::string, std::unordered_map<std::string, std::uint64_t>>();

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
                    if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) continue;
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
    if (!std::filesystem::is_regular_file(path, error)) return false;
    FileStat stat{};
    File::FillFileStat(path, &stat);
    bytes = static_cast<std::uint64_t>(stat.st_size);
    return true;
}

int _fail(int guestErrno) {
    *__error_nid_postfix() = guestErrno;
    return -1;
}

bool _resolve(const char* guestPath, std::uint32_t* id, std::uint64_t* size) {
    if (!guestPath) return false;
    std::filesystem::path hostPath;
    if (ResolvePathError_nid_no_patch(guestPath, hostPath) != 0) return false;
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
    std::uint32_t resolvedCount = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const std::string path = prefix ? (paths[index] ? std::string(prefix) + paths[index] : std::string()) : (paths[index] ? std::string(paths[index]) : std::string());
        const bool resolved = !path.empty() && _resolve(path.c_str(), &ids[index], sizes ? &sizes[index] : nullptr);
        if (!resolved) {
            ids[index] = INVALID_FILE_ID;
            if (sizes) sizes[index] = 0;
        }
        if (resolved) ++resolvedCount;
        if (results) results[index] = resolved ? 0 : SCE_KERNEL_ERROR_ENOENT;
    }
    return static_cast<int>(resolvedCount);
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
#ifdef _WIN32
    std::vector<char> staging(static_cast<std::size_t>(std::min<std::uint64_t>(command.size, 64u * 1024u)));
    std::uint64_t read = 0;
    while (read < command.size) {
        const auto wanted = static_cast<std::streamsize>(std::min<std::uint64_t>(command.size - read, staging.size()));
        stream.read(staging.data(), wanted);
        const auto obtained = static_cast<std::uint64_t>(stream.gcount());
        if (obtained != 0) std::memcpy(reinterpret_cast<char*>(command.destination) + read, staging.data(), static_cast<std::size_t>(obtained));
        read += obtained;
        if (stream.bad()) throw std::runtime_error("APR: read failed for " + file.path.string());
        if (obtained != static_cast<std::uint64_t>(wanted)) break;
    }
#else
    stream.read(reinterpret_cast<char*>(command.destination), static_cast<std::streamsize>(command.size));
    if (stream.bad()) throw std::runtime_error("APR: read failed for " + file.path.string());
    const auto read = static_cast<std::uint64_t>(stream.gcount());
#endif
    if (read != command.size && (!stream.eof() || command.offset + read < std::filesystem::file_size(file.path))) throw std::runtime_error("APR: read of " + file.path.string() + " at offset " + std::to_string(command.offset) + " returned " + std::to_string(read) + " of " + std::to_string(command.size) + " bytes");
    if (read != command.size) std::memset(reinterpret_cast<char*>(command.destination) + read, 0, static_cast<std::size_t>(command.size - read));
}

void _writeAddress(const Apr::WriteAddressCommand& command) {
    if (command.flags != 0) throw std::runtime_error("APR: WriteAddress flags " + std::to_string(command.flags) + " not implemented");
    std::atomic_ref<std::uint64_t>(*reinterpret_cast<std::uint64_t*>(command.address)).store(command.value, std::memory_order_release);
}

std::array<std::atomic<std::uint32_t>, 256> g_counters{};

std::mutex& g_counterWrites = *new std::mutex();

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
    Apr::CommandHeader header;
    std::memcpy(&header, buffer.base + cursor, sizeof(header));
    if (header.bytes < sizeof(TCommand)) throw std::runtime_error("APR: truncated command payload (opcode " + std::to_string(static_cast<std::uint32_t>(header.opcode)) + ", has " + std::to_string(header.bytes) + ", needs " + std::to_string(sizeof(TCommand)) + ")");
    TCommand command;
    std::memcpy(&command, buffer.base + cursor, sizeof(command));
    return command;
}

constexpr std::uint64_t AmmRangeBytes = 256ull << 30;
constexpr std::uint64_t AmmMultimapRangeBytes = 32ull << 30;
constexpr std::uint64_t AmmReservedBytes = AmmRangeBytes + AmmMultimapRangeBytes;
constexpr std::uint64_t AmmRangeAlignment = 64ull << 30;
constexpr int GuestMapFixed = 0x10;
constexpr int GuestMapNoOverwrite = 0x80;

static_assert(AmmPool::PageBytes == PS5_PAGE_SIZE);

using AmmPage = AmmPool::Page;

struct AmmState : AmmPool::Records {
    std::mutex lock;
    std::uintptr_t base = 0;
    std::set<std::uintptr_t> prt;
};

AmmState& _amm() {
    static auto& state = *new AmmState();
    return state;
}

std::uintptr_t _ammBase(AmmState& state) {
    if (state.base == 0) {
        std::uintptr_t arenaBase = 0;
        std::size_t arenaBytes = 0;
        GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
        if (arenaBase == 0 || arenaBytes < AmmReservedBytes) throw std::runtime_error("AMM: the guest address space arena cannot hold the virtual address range");
        const auto top = GuestArena::GuestArenaHighestFree_nid_no_patch(AmmReservedBytes, AmmRangeAlignment);
        if (top == 0) throw std::runtime_error("AMM: the guest address space arena has no free virtual address range");
        void* address = reinterpret_cast<void*>(top);
        if (DoReserveVirtual(&address, AmmReservedBytes, GuestMapFixed | GuestMapNoOverwrite, 0) != 0) throw std::runtime_error("AMM: cannot reserve the virtual address range");
        state.base = reinterpret_cast<std::uintptr_t>(address);
    }
    return state.base;
}

void _ammCheckRange(AmmState& state, std::uint64_t address, std::uint64_t size) {
    const auto base = _ammBase(state);
    if (address < base || size > AmmReservedBytes || address - base > AmmReservedBytes - size) throw std::runtime_error("AMM: range outside the AMM virtual address range");
}

int _ammHostProtection(std::int32_t protection) {
    int host = protection & 0x33;
    if ((protection & 0x140) != 0) host |= 1;
    if ((protection & 0x280) != 0) host |= 3;
    if ((host & 2) != 0) host |= 1;
    return host;
}

void _returnPoolPages(AmmState& state, std::uint64_t offset, std::uint64_t bytes) {
    AmmPool::ReturnPages(state, offset, bytes);
}

bool _tryTakePoolPages(AmmState& state, std::uint64_t bytes, std::uint64_t& offset) {
    return AmmPool::TryTakeRun(state, bytes, offset);
}

void _addPage(AmmState& state, std::uint64_t address, const AmmPage& page) {
    AmmPool::AddPage(state, address, page);
}

void _releasePage(AmmState& state, const AmmPage& page) {
    if (!page.pooled) return;
    const auto uses = state.pooledUses.find(page.physical);
    if (--uses->second == 0) {
        state.pooledUses.erase(uses);
        _returnPoolPages(state, page.physical, PS5_PAGE_SIZE);
    }
}

class AmmPageReferences {
public:
    AmmPageReferences(AmmState& state, const std::vector<AmmPage>& pages) : state(state), pages(pages) {
        for (const auto& page : pages) {
            if (page.pooled) ++state.pooledUses.find(page.physical)->second;
        }
    }

    AmmPageReferences(const AmmPageReferences&) = delete;
    AmmPageReferences& operator=(const AmmPageReferences&) = delete;

    ~AmmPageReferences() {
        for (const auto& page : pages) _releasePage(state, page);
    }

private:
    AmmState& state;
    const std::vector<AmmPage>& pages;
};

void _forgetPages(AmmState& state, std::uint64_t address, std::uint64_t size) {
    for (auto it = state.pages.lower_bound(address); it != state.pages.end() && it->first < address + size;) {
        _releasePage(state, it->second);
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
        for (std::size_t index = first; index < last; ++index) _addPage(state, address + index * PS5_PAGE_SIZE, pages[index]);
        first = last;
    }
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
    _forgetPages(state, command.address, command.size);
    _clearPrt(state, command.address, command.size);
    _reserve(command.address, command.size);
}

void _ammMapAsPrt(const Apr::AmmUnmapCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _forgetPages(state, command.address, command.size);
    _zeroPrt(command.address, command.size);
    for (std::uint64_t offset = 0; offset < command.size; offset += PS5_PAGE_SIZE) state.prt.insert(command.address + offset);
}

void _ammUnmapToPrt(const Apr::AmmUnmapCommand& command) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _requirePrt(state, command.address, command.size);
    _forgetPages(state, command.address, command.size);
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
            try {
                AmmPool::MapScattered(state, address, bytes, command.type, command.protection, [&](std::uint64_t at, std::uint64_t physical, std::uint64_t size) { _mapRun(at, physical, size, command.protection); });
            } catch (...) {
                _zeroPrt(address, bytes);
                throw;
            }
        }
        offset = end;
    }
}

void _ammMap(const Apr::AmmMapCommand& command, bool direct) {
    auto& state = _amm();
    const std::lock_guard lock(state.lock);
    _ammCheckRange(state, command.address, command.size);
    _reserve(command.address, command.size);
    _forgetPages(state, command.address, command.size);
    _clearPrt(state, command.address, command.size);
    std::uint64_t physical = direct ? command.directOffset : 0;
    if (!direct && !_tryTakePoolPages(state, command.size, physical)) {
        try {
            AmmPool::MapScattered(state, command.address, command.size, command.type, command.protection, [&](std::uint64_t address, std::uint64_t physicalRun, std::uint64_t size) { _mapRun(address, physicalRun, size, command.protection); });
        } catch (...) {
            _reserve(command.address, command.size);
            throw;
        }
        return;
    }
    try {
        _mapRun(command.address, physical, command.size, command.protection);
    } catch (...) {
        _reserve(command.address, command.size);
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
    const AmmPageReferences references(state, pages);
    if (!alias) {
        _reserve(command.source, command.size);
        _forgetPages(state, command.source, command.size);
        _clearPrt(state, command.source, command.size);
    }
    _reserve(command.address, command.size);
    _forgetPages(state, command.address, command.size);
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

struct Deferred {
    std::uint32_t cursor = 0;
    ReadCursor read;
};

std::optional<Deferred> _execute(const Apr::CommandBufferObject& buffer, std::uint32_t start = 0, ReadCursor read = {}, bool defer = false) {
    std::uint32_t cursor = start;
    while (cursor < buffer.offset) {
        if (buffer.offset - cursor < Apr::NopHeaderBytes) throw std::runtime_error("APR: truncated command buffer");
        std::uint32_t first;
        std::memcpy(&first, buffer.base + cursor, sizeof(first));
        if (Apr::IsNopHeader(first)) {
            const std::uint32_t bytes = Apr::NopDwords(first) * Apr::NopHeaderBytes;
            if (bytes > buffer.offset - cursor) throw std::runtime_error("APR: malformed command");
            cursor += bytes;
            continue;
        }
        if (cursor + sizeof(Apr::CommandHeader) > buffer.offset) throw std::runtime_error("APR: truncated command buffer");
        Apr::CommandHeader header;
        std::memcpy(&header, buffer.base + cursor, sizeof(header));
        if ((first & 0xffu) == static_cast<std::uint32_t>(Apr::Opcode::CompactReadFileGather)) {
            if (sizeof(Apr::CompactReadFileGatherCommand) > buffer.offset - cursor) throw std::runtime_error("APR: truncated compact gather command");
            Apr::CompactReadFileGatherCommand command;
            std::memcpy(&command, buffer.base + cursor, sizeof(command));
            const auto offset = static_cast<std::uint64_t>(command.opcodeAndOffsetHigh >> 8u) << 32u | command.offsetLow;
            if (offset >= 0x10000000000ull) throw std::runtime_error("APR: malformed compact gather offset");
            _readResolved(Apr::Opcode::ReadFileGather, Apr::ReadFileCommand{{}, 0, 0, 0, static_cast<std::uint64_t>(command.sizeMinusOne) + 1u, offset}, read);
            cursor += sizeof(command);
            continue;
        }
        if (header.bytes < sizeof(header) || header.bytes > buffer.offset - cursor) throw std::runtime_error("APR: malformed command");
        switch (header.opcode) {
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
            const auto command = _read<Apr::WriteAddressCommand>(buffer, cursor);
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
            if (defer && !_waitSatisfied(command.compare, (current() & command.mask) << unused, reference)) return Deferred{cursor, read};
            while (!_waitSatisfied(command.compare, (current() & command.mask) << unused, reference)) PreciseSleepUs(50);
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
    return std::nullopt;
}

}

extern "C" {

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
    if (multimapEnd) *multimapEnd = base + AmmReservedBytes;
}

struct AprQueue {
    std::mutex submit;
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable done;
    struct Work {
        std::vector<std::uint8_t> commands;
        Deferred resume;
        std::uint32_t id = 0;
    };
    std::deque<Work> pending;
    bool running = false;
    std::uint32_t submitted = 0;
    std::uint32_t completed = 0;
};

AprQueue* _startQueue() {
    auto* created = new AprQueue();
    std::thread([created] {
        for (;;) {
            AprQueue::Work work;
            {
                std::unique_lock lock(created->mutex);
                created->ready.wait(lock, [&] { return !created->pending.empty(); });
                work = std::move(created->pending.front());
                created->pending.pop_front();
                created->running = true;
            }
            try {
                const auto bytes = static_cast<std::uint32_t>(work.commands.size());
                _execute(Apr::CommandBufferObject{Apr::BufferType::Generic, 0, bytes, 0, bytes, work.commands.data()}, work.resume.cursor, work.resume.read);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "APR: %s\n", error.what());
                std::fflush(stderr);
                std::abort();
            }
            {
                std::lock_guard lock(created->mutex);
                created->running = false;
                created->completed = std::max(created->completed, work.id);
            }
            created->done.notify_all();
        }
    }).detach();
    return created;
}

AprQueue& _queue(bool amm) {
    static AprQueue* const queues[2] = {_startQueue(), _startQueue()};
    return *queues[amm ? 1 : 0];
}

std::uint32_t _submit(const std::uint8_t* base, std::uint32_t bytes, bool amm) {
    auto& queue = _queue(amm);
    std::lock_guard submitLock(queue.submit);
    std::uint32_t id;
    bool queued;
    {
        std::lock_guard lock(queue.mutex);
        id = ++queue.submitted;
        queued = queue.running || !queue.pending.empty();
    }
    const auto enqueue = [&](Deferred resume) {
        {
            std::lock_guard lock(queue.mutex);
            queue.pending.push_back({std::vector<std::uint8_t>(base, base + bytes), resume, id});
        }
        queue.ready.notify_one();
    };
    if (queued) {
        enqueue({});
        return id;
    }
    if (const auto deferred = _execute(Apr::CommandBufferObject{Apr::BufferType::Generic, 0, bytes, 0, bytes, const_cast<std::uint8_t*>(base)}, 0, {}, true)) {
        enqueue(*deferred);
        return id;
    }
    {
        std::lock_guard lock(queue.mutex);
        queue.completed = std::max(queue.completed, id);
    }
    queue.done.notify_all();
    return id;
}

void _waitSubmitted(std::uint32_t id, bool amm) {
    auto& queue = _queue(amm);
    std::unique_lock lock(queue.mutex);
    queue.done.wait(lock, [&] { return queue.completed >= id || id > queue.submitted; });
}

std::uint32_t AmmSubmit_nid_no_patch(void* base, std::uint32_t bytes) {
    return _submit(static_cast<const std::uint8_t*>(base), bytes, true);
}

bool AmmSubmitted_nid_no_patch(std::uint32_t id) {
    if (id == 0) return false;
    {
        auto& queue = _queue(true);
        std::lock_guard lock(queue.mutex);
        if (id > queue.submitted) return false;
    }
    _waitSubmitted(id, true);
    return true;
}

int APS5_VABI sceKernelAprResolveFilepathsToIds(const char** paths, uint32_t count, uint32_t* ids, uint32_t* error_index) {
    if (!paths || !ids) return _fail(GUEST_EINVAL);
    for (uint32_t index = 0; index < count; ++index) {
        if (!_resolve(paths[index], &ids[index], nullptr)) {
            ids[index] = INVALID_FILE_ID;
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
            ids[index] = INVALID_FILE_ID;
            if (sizes) sizes[index] = 0;
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
            ids[index] = INVALID_FILE_ID;
            if (sizes) sizes[index] = 0;
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
    if (!buffer) return _fail(GUEST_EINVAL);
    _submit(buffer->base, buffer->offset, false);
    return 0;
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetId(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* id) {
    if (!id) return _fail(GUEST_EINVAL);
    (void)priority;
    if (!buffer) return _fail(GUEST_EINVAL);
    *id = _submit(buffer->base, buffer->offset, false);
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
    _waitSubmitted(id, false);
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

bool _validWriteRange(std::uint64_t address, std::uint64_t size) {
    constexpr std::uint64_t pageMask = 0x3FFFu;
    return address != 0u && size != 0u && ((address | size) & pageMask) == 0u && address + size > address;
}

bool _validWriteProtection(std::uint64_t protection) {
    return (protection & ~0x3F3ULL) == 0u;
}

int APS5_VABI sceKernelWriteModifyMtypeProtectCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    if (!_validWriteRange(address, size) || !_validWriteProtection(protection) || !_validWriteProtection(mask)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmProtectCommand command{{Apr::Opcode::AmmModifyMtypeProtect, sizeof(command)}, address, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask), 0};
    if (dst) std::memcpy(dst, &command, sizeof(command));
    if (outSize) *outSize = sizeof(command);
    return 0;
}

int APS5_VABI sceKernelWriteRemapIntoPrtCommand(std::uint32_t* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, int protection, int opcode, std::uint64_t* outSize) {
    if (!_validWriteRange(address, size) || !_validWriteRange(source, size) || !_validWriteProtection(static_cast<std::uint32_t>(protection))) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmRemapCommand command{{Apr::Opcode::AmmRemapIntoPrt, sizeof(command)}, address, source, size, protection, opcode ? opcode : 1011};
    if (dst) std::memcpy(dst, &command, sizeof(command));
    if (outSize) *outSize = sizeof(command);
    return 0;
}

}

#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_SRC_MAPPEDCACHEFILE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_SRC_MAPPEDCACHEFILE_HPP

#ifdef _WIN32
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace AgcDriver::Graphics {

class MappedCacheFile {
public:
    static constexpr std::size_t TrimmedFromBytes = std::size_t{64} << 20u;

    static std::unique_ptr<MappedCacheFile> Create(const std::filesystem::path& destination, std::size_t bytes, std::size_t window, const std::function<std::thread(std::function<void()>)>& startThread) {
        if (bytes == 0) return nullptr;
        std::unique_ptr<MappedCacheFile> created(new MappedCacheFile());
        std::error_code error;
        std::filesystem::create_directories(destination.parent_path(), error);
        static std::atomic<std::uint64_t> serial{0};
        created->destination = destination;
        created->temporary = destination;
        created->temporary += L".tmp.map." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetCurrentThreadId()) + L"." + std::to_wstring(serial.fetch_add(1, std::memory_order_relaxed));
        created->file = CreateFileW(created->temporary.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (created->file == INVALID_HANDLE_VALUE) return nullptr;
        created->mapping = CreateFileMappingW(created->file, nullptr, PAGE_READWRITE, static_cast<DWORD>(static_cast<std::uint64_t>(bytes) >> 32u), static_cast<DWORD>(bytes & 0xffffffffu), nullptr);
        if (created->mapping == nullptr) return nullptr;
        created->view = static_cast<std::byte*>(MapViewOfFile(created->mapping, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, bytes));
        if (created->view == nullptr) return nullptr;
        created->bytes = bytes;
        created->opened.store(bytes, std::memory_order_relaxed);
        if (window != 0) {
            SYSTEM_INFO system{};
            GetSystemInfo(&system);
            const std::size_t granule = std::max<std::size_t>(system.dwPageSize, 4096u);
            window = (window + granule - 1) / granule * granule;
            if (window < bytes) {
                DWORD previous = 0;
                if (!enroll(created.get()) || refuseProtection().load(std::memory_order_relaxed) || !VirtualProtect(created->view + window, bytes - window, PAGE_NOACCESS, &previous)) return nullptr;
                created->window = window;
                created->opened.store(window, std::memory_order_release);
            }
        }
        if (startThread && bytes >= TrimmedFromBytes) {
            auto* self = created.get();
            created->trimmer = startThread([self] { self->trim(); });
        }
        return created;
    }

    MappedCacheFile(const MappedCacheFile&) = delete;
    MappedCacheFile& operator=(const MappedCacheFile&) = delete;

    ~MappedCacheFile() {
        release();
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (!committed && !temporary.empty()) DeleteFileW(temporary.c_str());
    }

    std::byte* Data() const { return view; }
    std::uint64_t WindowsOpened() const { return windowsOpened.load(std::memory_order_relaxed); }

    bool Commit(std::size_t finalBytes) {
        if (view == nullptr || finalBytes == 0 || finalBytes > bytes) return false;
        stopTrimmer();
        withdraw(this);
        DWORD previous = 0;
        if (window != 0 && !VirtualProtect(view, bytes, PAGE_READWRITE, &previous)) return false;
        const bool flushed = FlushViewOfFile(view, finalBytes) != 0;
        release();
        if (!flushed) return false;
        LARGE_INTEGER end{};
        end.QuadPart = static_cast<LONGLONG>(finalBytes);
        if (!SetFilePointerEx(file, end, nullptr, FILE_BEGIN) || !SetEndOfFile(file) || !FlushFileBuffers(file)) return false;
        struct RenameInfo {
            DWORD flags;
            HANDLE root;
            DWORD nameBytes;
            WCHAR name[1];
        };
        const auto nameBytes = destination.native().size() * sizeof(wchar_t);
        std::string buffer(sizeof(RenameInfo) + nameBytes, '\0');
        auto* rename = reinterpret_cast<RenameInfo*>(buffer.data());
        constexpr DWORD replaceExisting = 0x00000001, posixSemantics = 0x00000002;
        rename->flags = replaceExisting | posixSemantics;
        rename->nameBytes = static_cast<DWORD>(nameBytes);
        std::memcpy(rename->name, destination.c_str(), nameBytes);
        constexpr auto renameInfoEx = static_cast<FILE_INFO_BY_HANDLE_CLASS>(22);
        if (SetFileInformationByHandle(file, renameInfoEx, rename, static_cast<DWORD>(buffer.size()))) {
            committed = true;
        } else {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
            committed = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        }
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
        return committed;
    }

    friend struct MappedCacheFileTestAccess;

private:
    MappedCacheFile() = default;

    static std::atomic<bool>& refuseProtection() {
        static std::atomic<bool> refuse{false};
        return refuse;
    }

    struct Registry {
        SRWLOCK lock = SRWLOCK_INIT;
        std::array<MappedCacheFile*, 4> files{};
        std::atomic<std::uint32_t> enrolled{0};
        void* handler = nullptr;
    };

    static Registry& registry() {
        static auto* shared = new Registry();
        return *shared;
    }

    static bool enroll(MappedCacheFile* file) {
        auto& shared = registry();
        AcquireSRWLockExclusive(&shared.lock);
        if (shared.handler == nullptr) shared.handler = AddVectoredExceptionHandler(1, &MappedCacheFile::onFault);
        bool placed = false;
        if (shared.handler != nullptr) {
            for (auto& slot : shared.files) {
                if (slot != nullptr) continue;
                slot = file;
                shared.enrolled.fetch_add(1, std::memory_order_release);
                placed = true;
                break;
            }
        }
        ReleaseSRWLockExclusive(&shared.lock);
        return placed;
    }

    static void withdraw(MappedCacheFile* file) {
        auto& shared = registry();
        AcquireSRWLockExclusive(&shared.lock);
        for (auto& slot : shared.files) {
            if (slot != file) continue;
            slot = nullptr;
            shared.enrolled.fetch_sub(1, std::memory_order_release);
        }
        ReleaseSRWLockExclusive(&shared.lock);
    }

    static LONG CALLBACK onFault(EXCEPTION_POINTERS* info) {
        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || info->ExceptionRecord->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
        const auto address = static_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionInformation[1]);
        auto& shared = registry();
        if (shared.enrolled.load(std::memory_order_acquire) == 0) return EXCEPTION_CONTINUE_SEARCH;
        LONG verdict = EXCEPTION_CONTINUE_SEARCH;
        AcquireSRWLockShared(&shared.lock);
        for (auto* file : shared.files) {
            if (file == nullptr) continue;
            const auto base = reinterpret_cast<std::uintptr_t>(file->view);
            if (address < base || address - base >= file->bytes) continue;
            if (file->openThrough(address - base)) verdict = EXCEPTION_CONTINUE_EXECUTION;
            break;
        }
        ReleaseSRWLockShared(&shared.lock);
        return verdict;
    }

    bool openThrough(std::size_t offset) {
        AcquireSRWLockExclusive(&gate);
        const bool mended = openThroughLocked(offset);
        ReleaseSRWLockExclusive(&gate);
        return mended;
    }

    bool openThroughLocked(std::size_t offset) {
        const auto open = opened.load(std::memory_order_relaxed);
        if (offset < open) {
            if (lastFault == offset && ++repeats > 16u) return false;
            if (lastFault != offset) repeats = 0;
            lastFault = offset;
            return true;
        }
        if (open > written) {
            FlushViewOfFile(view + written, open - written);
            VirtualUnlock(view + written, open - written);
            written = open;
        }
        const auto end = std::min(bytes, (offset / window + 1) * window);
        DWORD previous = 0;
        if (!VirtualProtect(view + open, end - open, PAGE_READWRITE, &previous)) return false;
        opened.store(end, std::memory_order_release);
        windowsOpened.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void stopTrimmer() {
        stop.store(true, std::memory_order_release);
        if (trimmer.joinable()) trimmer.join();
    }

    void release() {
        stopTrimmer();
        withdraw(this);
        if (view != nullptr) UnmapViewOfFile(view);
        view = nullptr;
        if (mapping != nullptr) CloseHandle(mapping);
        mapping = nullptr;
    }

    void trim() {
        constexpr std::size_t Step = std::size_t{64} << 20u;
        while (!stop.load(std::memory_order_acquire)) {
            const auto end = opened.load(std::memory_order_acquire);
            for (std::size_t at = 0; at < end && !stop.load(std::memory_order_acquire); at += Step) {
                const auto length = std::min(Step, end - at);
                FlushViewOfFile(view + at, length);
                VirtualUnlock(view + at, length);
            }
            for (int wait = 0; wait < 5 && !stop.load(std::memory_order_acquire); ++wait) Sleep(10);
        }
    }

    std::filesystem::path destination;
    std::filesystem::path temporary;
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE mapping = nullptr;
    SRWLOCK gate = SRWLOCK_INIT;
    std::byte* view = nullptr;
    std::size_t bytes = 0;
    std::size_t window = 0;
    std::size_t written = 0;
    std::size_t lastFault = ~std::size_t{0};
    std::uint32_t repeats = 0;
    bool committed = false;
    std::atomic<std::size_t> opened{0};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> windowsOpened{0};
    std::thread trimmer;
};

struct MappedCacheFileTestAccess {
    static void RefuseWindowProtection(bool refuse) {
        MappedCacheFile::refuseProtection().store(refuse, std::memory_order_relaxed);
    }
};

}
#endif

#endif

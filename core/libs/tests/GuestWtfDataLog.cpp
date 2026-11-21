#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/include/WtfDataFileExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Jsc::WtfAbi::FilePrintStream;
using Jsc::WtfAbi::FilePrintStreamPtr;
using Jsc::WtfAbi::PrintStream;
using Jsc::WtfAbi::PrintStreamPtr;

using Destructor = void (APS5_VABI*)(PrintStream*);
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Flush = void (APS5_VABI*)(PrintStream*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

static_assert(Jsc::WtfAbi::PrintStreamCompleteDestructor == 0 && Jsc::WtfAbi::PrintStreamDeletingDestructor == 1);
static_assert(Jsc::WtfAbi::PrintStreamVprintf == 2 && Jsc::WtfAbi::PrintStreamFlush == 3);
static_assert(Jsc::WtfAbi::PrintStreamBegin == 4 && Jsc::WtfAbi::PrintStreamEnd == 5);
static_assert(Jsc::WtfAbi::FilePrintStreamAdopt == 0 && Jsc::WtfAbi::FilePrintStreamBorrow == 1);

std::mutex heapLock;
std::array<void*, 64> released{};
std::size_t releaseCount = 0;

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes == 0 ? 1 : bytes); }
void APS5_VABI HeapFree(void* pointer) {
    {
        std::lock_guard guard(heapLock);
        released[releaseCount++ % released.size()] = pointer;
    }
    std::free(pointer);
}
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count == 0 ? 1 : count, bytes == 0 ? 1 : bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes == 0 ? 1 : bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t) { throw std::logic_error("guest_wtf_data_log: unexpected aligned allocation"); }
void* APS5_VABI HeapRealign(void*, std::size_t, std::size_t) { throw std::logic_error("guest_wtf_data_log: unexpected aligned reallocation"); }
int APS5_VABI HeapPosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("guest_wtf_data_log: unexpected posix_memalign"); }

void RegisterTrackingHeap() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&HeapAllocate);
    api[1] = reinterpret_cast<void*>(&HeapFree);
    api[2] = reinterpret_cast<void*>(&HeapCalloc);
    api[3] = reinterpret_cast<void*>(&HeapReallocate);
    api[4] = reinterpret_cast<void*>(&HeapAlign);
    api[5] = reinterpret_cast<void*>(&HeapRealign);
    api[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

std::size_t ReleaseMark() {
    std::lock_guard guard(heapLock);
    return releaseCount;
}

bool ReleasedSince(std::size_t mark, const void* pointer) {
    std::lock_guard guard(heapLock);
    for (std::size_t index = mark; index < releaseCount; ++index) {
        if (released[index % released.size()] == pointer) return true;
    }
    return false;
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TFunction>
TFunction Entry(PrintStream* stream, std::size_t slot) {
    Require(stream && stream->vtable && stream->vtable[slot], "missing virtual function");
    return reinterpret_cast<TFunction>(const_cast<void*>(stream->vtable[slot]));
}

PrintStream* AsStream(FilePrintStream* stream) {
    return reinterpret_cast<PrintStream*>(stream);
}

void FlushStream(PrintStream* stream) {
    Entry<Flush>(stream, 3)(stream);
}

void APS5_VABI Print(PrintStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    Entry<Vprintf>(stream, 2)(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string GuestPath(const char* name) {
    return std::string("/wtf-data-log/") + name;
}

FilePrintStream* Open(const char* name) {
    const std::string path = GuestPath(name);
    FilePrintStreamPtr result{reinterpret_cast<FilePrintStream*>(1)};
    Require(_ZN3WTF15FilePrintStream4openEPKcS2_(&result, path.c_str(), "w") == &result, "open returns its result slot");
    Require(result.pointer != nullptr, "open failed");
    Require(result.pointer->vtable != nullptr && result.pointer->file != nullptr, "open stream fields");
    Require(result.pointer->adoptionMode == 0, "open adopts the FILE (Adopt = 0)");
    return result.pointer;
}

bool HostDescriptorClosed(int descriptor, const std::filesystem::path& path) {
#ifdef _WIN32
    static_cast<void>(descriptor);
    std::error_code error;
    return std::filesystem::remove(path, error);
#else
    static_cast<void>(path);
    errno = 0;
    return ::fcntl(descriptor, F_GETFD) == -1 && errno == EBADF;
#endif
}

template <typename TCall>
bool ThrowsLogicError(TCall call) {
    try {
        call();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

template <typename TCall>
bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename TValue>
TValue* ReadOnlyCopy(const TValue& value) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, sizeof(value), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr, "read-only page allocation");
    std::memcpy(page, &value, sizeof(value));
    DWORD previous = 0;
    Require(VirtualProtect(page, sizeof(value), PAGE_READONLY, &previous) != 0, "read-only page protection");
#else
    void* page = mmap(nullptr, sizeof(value), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED, "read-only page allocation");
    std::memcpy(page, &value, sizeof(value));
    Require(mprotect(page, sizeof(value), PROT_READ) == 0, "read-only page protection");
#endif
    return static_cast<TValue*>(page);
}

void CheckVtableLayout() {
    FilePrintStream* stream = Open("layout.txt");
    const void* const* vtable = stream->vtable;
    Require(vtable[0] != nullptr && vtable[1] != nullptr && vtable[0] != vtable[1], "slots 0 and 1 are distinct destructors");
    Require(vtable[-2] == nullptr && vtable[-1] == nullptr, "offset-to-top and RTTI precede slot 0");
    for (std::size_t slot = 2; slot < 6; ++slot) Require(vtable[slot] != nullptr, "slots 2 to 5 are vprintf, flush, begin, end");
    Require(Entry<Begin>(AsStream(stream), 4)(AsStream(stream)) == AsStream(stream), "slot 4 begin returns the stream");
    Entry<End>(AsStream(stream), 5)(AsStream(stream));
    _ZN3WTF15FilePrintStreamD0Ev(stream);
}

std::string CaptureStdout(void (*print)()) {
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-wtf-data-log-stdout-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::fflush(stdout);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stdout));
    std::FILE* capture = std::fopen(path.string().c_str(), "wb");
    Require(saved >= 0 && capture != nullptr, "stdout capture setup");
    Require(::_dup2(::_fileno(capture), ::_fileno(stdout)) == 0, "redirect stdout");
#else
    const int saved = ::dup(::fileno(stdout));
    std::FILE* capture = std::fopen(path.string().c_str(), "wb");
    Require(saved >= 0 && capture != nullptr, "stdout capture setup");
    Require(::dup2(::fileno(capture), ::fileno(stdout)) >= 0, "redirect stdout");
#endif
    print();
    std::fflush(stdout);
#ifdef _WIN32
    ::_dup2(saved, ::_fileno(stdout));
    ::_close(saved);
#else
    ::dup2(saved, ::fileno(stdout));
    ::close(saved);
#endif
    std::fclose(capture);
    std::string text = ReadAll(path);
    std::error_code error;
    std::filesystem::remove(path, error);
    return text;
}

void PrintThroughDefaultSink() {
    PrintStream* stream = &_ZN3WTF8dataFileEv();
    Print(stream, "guest_wtf_data_log: %s %d\n", "default sink", 1);
    FlushStream(stream);
}

void CheckDefaultSink() {
    const std::string printed = CaptureStdout(PrintThroughDefaultSink);
    Require(printed == "guest_wtf_data_log: default sink 1\n", "the default sink writes to stdout");
    PrintStream& first = _ZN3WTF8dataFileEv();
    Require(&first == &_ZN3WTF8dataFileEv(), "dataFile is one stream");
    Require(&first == WtfDataFile_nid_no_patch(), "libSceJsc owns the data file");
    PrintStream* target = Entry<Begin>(&first, 4)(&first);
    Require(target != &first, "begin returns the locked target");
    PrintStream* again = Entry<Begin>(&first, 4)(&first);
    Require(again == target, "begin is recursive on one thread");
    auto* file = reinterpret_cast<FilePrintStream*>(target);
    Require(file->file == &_Stdout_nid_postfix, "default sink is guest stdout");
    Require(file->adoptionMode == 1, "default sink borrows stdout (Borrow = 1)");
    Require(Entry<Begin>(target, 4)(target) == target, "FilePrintStream begin returns itself");
    Entry<End>(target, 5)(target);
    Entry<End>(&first, 5)(&first);
    Entry<End>(&first, 5)(&first);
}

void CheckStaticStreamIsNotDestroyed() {
    PrintStream* stream = &_ZN3WTF8dataFileEv();
    Require(ThrowsLogicError([stream] { Entry<Destructor>(stream, 0)(stream); }), "slot 0 on the static locked stream throws");
    Require(ThrowsLogicError([stream] { Entry<Destructor>(stream, 1)(stream); }), "slot 1 on the static locked stream throws");
    Require(&_ZN3WTF8dataFileEv() == stream, "the static stream stays installed");
    PrintStream* target = Entry<Begin>(stream, 4)(stream);
    Require(reinterpret_cast<FilePrintStream*>(target)->file == &_Stdout_nid_postfix, "the static stream keeps its target");
    Entry<End>(stream, 5)(stream);
}

void CheckLockExcludesOtherThreads() {
    PrintStream& stream = _ZN3WTF8dataFileEv();
    Entry<Begin>(&stream, 4)(&stream);
    std::atomic<bool> flushed{false};
    std::thread other([&] {
        FlushStream(&stream);
        flushed.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool flushedWhileLocked = flushed.load();
    Entry<End>(&stream, 5)(&stream);
    other.join();
    Require(!flushedWhileLocked, "flush waits for the lock held by begin");
    Require(flushed.load(), "flush runs after end");
}

void CheckDeletingDestructor(const std::filesystem::path& directory, const char* name, void (*destroy)(FilePrintStream*)) {
    const auto path = directory / name;
    FilePrintStream* stream = Open(name);
    Print(AsStream(stream), "%s=%d;%.1f", "value", 42, 2.5);
    FlushStream(AsStream(stream));
    Require(ReadAll(path) == "value=42;2.5", "vprintf writes through vfprintf");
    Print(AsStream(stream), ";unflushed");
    const int descriptor = fileno_nid_postfix(stream->file);
    Require(descriptor >= 0, "open stream has a descriptor");
    const std::size_t mark = ReleaseMark();
    destroy(stream);
    Require(ReleasedSince(mark, stream), "the deleting destructor frees the stream");
    Require(HostDescriptorClosed(descriptor, path), "the deleting destructor closes the FILE");
#ifndef _WIN32
    Require(ReadAll(path) == "value=42;2.5;unflushed", "the deleting destructor flushes on close");
#endif
}

void DestroyD0Export(FilePrintStream* stream) {
    _ZN3WTF15FilePrintStreamD0Ev(stream);
}

void DestroySlot1(FilePrintStream* stream) {
    Entry<Destructor>(AsStream(stream), 1)(AsStream(stream));
}

void CheckCompleteDestructorClosesAdoptedFile(const std::filesystem::path& directory, const char* name, void (*destroy)(FilePrintStream*)) {
    const auto path = directory / name;
    FilePrintStream* stream = Open(name);
    Print(AsStream(stream), "closed by %s", name);
    const int descriptor = fileno_nid_postfix(stream->file);
    const std::size_t mark = ReleaseMark();
    destroy(stream);
    Require(!ReleasedSince(mark, stream), "a non-deleting destructor keeps the object");
    Require(HostDescriptorClosed(descriptor, path), "a non-deleting destructor fcloses an adopted FILE");
#ifndef _WIN32
    Require(ReadAll(path) == std::string("closed by ") + name, "fclose flushes unflushed output");
#endif
    _ZdlPv_nid_postfix(stream);
}

void DestroyD1Export(FilePrintStream* stream) {
    _ZN3WTF15FilePrintStreamD1Ev(stream);
}

void DestroyD2Export(FilePrintStream* stream) {
    _ZN3WTF15FilePrintStreamD2Ev(stream);
}

void DestroySlot0(FilePrintStream* stream) {
    Entry<Destructor>(AsStream(stream), 0)(AsStream(stream));
}

void CheckBorrowedFileStaysOpen(const std::filesystem::path& directory, const char* name, void (*destroy)(FilePrintStream*), bool deleting) {
    const auto path = directory / name;
    FilePrintStream* stream = Open(name);
    FileStream* file = stream->file;
    stream->adoptionMode = 1;
    Print(AsStream(stream), "before;");
    const std::size_t mark = ReleaseMark();
    destroy(stream);
    Require(ReleasedSince(mark, stream) == deleting, "only the deleting destructor frees a borrowing stream");
    Require(fputs_nid_postfix("after", file) >= 0, "borrowed FILE stays open");
    Require(fclose_nid_postfix(file) == 0, "borrowed FILE closes once");
    Require(ReadAll(path) == "before;after", "borrowed FILE output");
    if (!deleting) _ZdlPv_nid_postfix(stream);
}

void CheckOpenFailure() {
    const std::string path = GuestPath("missing/file.txt");
    FilePrintStreamPtr result{reinterpret_cast<FilePrintStream*>(1)};
    Require(_ZN3WTF15FilePrintStream4openEPKcS2_(&result, path.c_str(), "w") == &result, "failed open returns its result slot");
    Require(result.pointer == nullptr, "failed fopen gives a null stream");
}

void CheckUnusableGuestPointers(const std::filesystem::path& directory) {
    errno = 71;
    Require(ThrowsInvalidArgument([] {
        _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(reinterpret_cast<PrintStreamPtr*>(0x10));
    }), "setDataFile rejects an unreadable unique_ptr");
    Require(errno == 71, "setDataFile keeps errno for an unreadable unique_ptr");

    FilePrintStream* stream = Open("read-only-owner.txt");
    PrintStreamPtr* owner = ReadOnlyCopy(PrintStreamPtr{AsStream(stream)});
    errno = 71;
    Require(ThrowsInvalidArgument([owner] {
        _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(owner);
    }), "setDataFile rejects a read-only unique_ptr");
    Require(errno == 71, "setDataFile keeps errno for a read-only unique_ptr");
    Require(owner->pointer == AsStream(stream), "a rejected setDataFile keeps a read-only owner unchanged");
    Require(&_ZN3WTF8dataFileEv() != AsStream(stream), "a rejected setDataFile installs nothing");
    _ZN3WTF15FilePrintStreamD0Ev(stream);

    const std::string path = GuestPath("unusable-result.txt");
    errno = 71;
    Require(ThrowsInvalidArgument([&path] {
        _ZN3WTF15FilePrintStream4openEPKcS2_(reinterpret_cast<FilePrintStreamPtr*>(0x10), path.c_str(), "w");
    }), "open rejects an unmapped result");
    Require(errno == 71, "open keeps errno for an unmapped result");
    FilePrintStreamPtr* result = ReadOnlyCopy(FilePrintStreamPtr{reinterpret_cast<FilePrintStream*>(1)});
    errno = 71;
    Require(ThrowsInvalidArgument([result, &path] {
        _ZN3WTF15FilePrintStream4openEPKcS2_(result, path.c_str(), "w");
    }), "open rejects a read-only result");
    Require(errno == 71, "open keeps errno for a read-only result");
    Require(result->pointer == reinterpret_cast<FilePrintStream*>(1), "a rejected open leaves a read-only result unchanged");
    Require(!std::filesystem::exists(directory / "unusable-result.txt"), "a rejected open creates no file");
}

void InstallDataFile(PrintStream* stream) {
    PrintStreamPtr owner{stream};
    _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(&owner);
    Require(owner.pointer == nullptr, "setDataFile takes ownership");
}

void CheckInstalledStream(const std::filesystem::path& directory, FilePrintStream* installed, const char* name) {
    PrintStream& stream = _ZN3WTF8dataFileEv();
    Require(&stream == AsStream(installed), "dataFile returns the installed stream");
    Print(&stream, "routed %d", 7);
    FlushStream(&stream);
    Require(ReadAll(directory / name) == "routed 7", "dataFile output reaches the installed stream");

    FilePrintStream* second = Open("second.txt");
    PrintStreamPtr secondOwner{AsStream(second)};
    Require(ThrowsLogicError([&secondOwner] {
        _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(&secondOwner);
    }), "a second setDataFile fails the release assertion");
    Require(secondOwner.pointer == AsStream(second), "a rejected setDataFile keeps ownership with the caller");
    Require(&_ZN3WTF8dataFileEv() == AsStream(installed), "a rejected setDataFile keeps the installed stream");
    _ZN3WTF15FilePrintStreamD0Ev(second);
}

void CheckSetDataFileAfterDefault(const std::filesystem::path& directory) {
    FilePrintStream* installed = Open("installed.txt");
    InstallDataFile(AsStream(installed));
    CheckInstalledStream(directory, installed, "installed.txt");
}

void RunDefault(const std::filesystem::path& directory) {
    CheckDefaultSink();
    CheckStaticStreamIsNotDestroyed();
    CheckLockExcludesOtherThreads();
    CheckVtableLayout();
    CheckDeletingDestructor(directory, "d0.txt", DestroyD0Export);
    CheckDeletingDestructor(directory, "slot1.txt", DestroySlot1);
    CheckCompleteDestructorClosesAdoptedFile(directory, "d1.txt", DestroyD1Export);
    CheckCompleteDestructorClosesAdoptedFile(directory, "d2.txt", DestroyD2Export);
    CheckCompleteDestructorClosesAdoptedFile(directory, "slot0.txt", DestroySlot0);
    CheckBorrowedFileStaysOpen(directory, "borrow-d1.txt", DestroyD1Export, false);
    CheckBorrowedFileStaysOpen(directory, "borrow-d2.txt", DestroyD2Export, false);
    CheckBorrowedFileStaysOpen(directory, "borrow-slot0.txt", DestroySlot0, false);
    CheckBorrowedFileStaysOpen(directory, "borrow-d0.txt", DestroyD0Export, true);
    CheckBorrowedFileStaysOpen(directory, "borrow-slot1.txt", DestroySlot1, true);
    CheckOpenFailure();
    CheckUnusableGuestPointers(directory);
    CheckSetDataFileAfterDefault(directory);
}

void RunInstallFirst(const std::filesystem::path& directory) {
    FilePrintStream* installed = Open("first.txt");
    InstallDataFile(AsStream(installed));
    CheckInstalledStream(directory, installed, "first.txt");
}

void RunInstallNull() {
    InstallDataFile(nullptr);
    PrintStream* stream = &_ZN3WTF8dataFileEv();
    PrintStream* target = Entry<Begin>(stream, 4)(stream);
    Require(reinterpret_cast<FilePrintStream*>(target)->file == &_Stdout_nid_postfix, "a null stream installed first gives the default sink");
    Entry<End>(stream, 5)(stream);
    InstallDataFile(nullptr);
    Require(ThrowsLogicError([] { _ZN3WTF8dataFileEv(); }), "dataFile throws after a null stream replaced the default sink");
}

} // namespace

int main(int argc, char** argv) {
    RegisterTrackingHeap();
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-wtf-data-log-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "temporary directory");
    AddPathAlias_nid_no_patch("/wtf-data-log", directory.string().c_str());
    const std::string mode = argc > 1 ? argv[1] : "default";
    if (mode == "default") {
        RunDefault(directory);
    } else if (mode == "install-first") {
        RunInstallFirst(directory);
    } else if (mode == "install-null") {
        RunInstallNull();
    } else {
        throw std::invalid_argument("guest_wtf_data_log: unknown mode");
    }
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    return EXIT_SUCCESS;
}

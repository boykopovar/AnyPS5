#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/include/WtfDataFileExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using Jsc::WtfAbi::FilePrintStream;
using Jsc::WtfAbi::FilePrintStreamPtr;
using Jsc::WtfAbi::PrintStream;
using Jsc::WtfAbi::PrintStreamPtr;

using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Flush = void (APS5_VABI*)(PrintStream*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TFunction>
TFunction Slot(PrintStream* stream, Jsc::WtfAbi::PrintStreamSlot slot) {
    Require(stream && stream->vtable && stream->vtable[slot], "missing virtual function");
    return reinterpret_cast<TFunction>(const_cast<void*>(stream->vtable[slot]));
}

PrintStream* AsStream(FilePrintStream* stream) {
    return reinterpret_cast<PrintStream*>(stream);
}

void APS5_VABI Print(PrintStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    Slot<Vprintf>(stream, Jsc::WtfAbi::PrintStreamVprintf)(stream, format, reinterpret_cast<VaList*>(args));
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
    Require(result.pointer->adoptionMode == Jsc::WtfAbi::FilePrintStreamAdopt, "open adopts the FILE");
    return result.pointer;
}

void CheckDefaultSink() {
    PrintStream& first = _ZN3WTF8dataFileEv();
    Require(&first == &_ZN3WTF8dataFileEv(), "dataFile is one stream");
    Require(&first == WtfDataFile_nid_no_patch(), "libSceJsc owns the data file");
    PrintStream* target = Slot<Begin>(&first, Jsc::WtfAbi::PrintStreamBegin)(&first);
    Require(target != &first, "begin returns the locked target");
    PrintStream* again = Slot<Begin>(&first, Jsc::WtfAbi::PrintStreamBegin)(&first);
    Require(again == target, "begin is recursive on one thread");
    auto* file = reinterpret_cast<FilePrintStream*>(target);
    Require(file->file == &_Stdout_nid_postfix, "default sink is guest stdout");
    Require(file->adoptionMode == Jsc::WtfAbi::FilePrintStreamBorrow, "default sink borrows stdout");
    Require(Slot<Begin>(target, Jsc::WtfAbi::PrintStreamBegin)(target) == target, "FilePrintStream begin returns itself");
    Slot<End>(target, Jsc::WtfAbi::PrintStreamEnd)(target);
    Slot<End>(&first, Jsc::WtfAbi::PrintStreamEnd)(&first);
    Slot<End>(&first, Jsc::WtfAbi::PrintStreamEnd)(&first);
    Print(&first, "guest_wtf_data_log: %s %d\n", "default sink", 1);
    Slot<Flush>(&first, Jsc::WtfAbi::PrintStreamFlush)(&first);
}

void CheckLockExcludesOtherThreads() {
    PrintStream& stream = _ZN3WTF8dataFileEv();
    Slot<Begin>(&stream, Jsc::WtfAbi::PrintStreamBegin)(&stream);
    std::atomic<bool> flushed{false};
    std::thread other([&] {
        Slot<Flush>(&stream, Jsc::WtfAbi::PrintStreamFlush)(&stream);
        flushed.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool flushedWhileLocked = flushed.load();
    Slot<End>(&stream, Jsc::WtfAbi::PrintStreamEnd)(&stream);
    other.join();
    Require(!flushedWhileLocked, "flush waits for the lock held by begin");
    Require(flushed.load(), "flush runs after end");
}

void CheckOpenWriteAndDelete(const std::filesystem::path& directory) {
    const auto path = directory / "deleting.txt";
    FilePrintStream* stream = Open("deleting.txt");
    Print(AsStream(stream), "%s=%d;%.1f", "value", 42, 2.5);
    Slot<Flush>(AsStream(stream), Jsc::WtfAbi::PrintStreamFlush)(AsStream(stream));
    Require(ReadAll(path) == "value=42;2.5", "vprintf writes through vfprintf");
    _ZN3WTF15FilePrintStreamD0Ev(stream);
    Require(ReadAll(path) == "value=42;2.5", "deleting destructor keeps the output");
}

void CheckBaseDestructorClosesAdoptedFile(const std::filesystem::path& directory) {
    const auto path = directory / "base.txt";
    FilePrintStream* stream = Open("base.txt");
    Print(AsStream(stream), "closed by %s", "D2");
    _ZN3WTF15FilePrintStreamD2Ev(stream);
    Require(ReadAll(path) == "closed by D2", "base destructor fcloses an adopted FILE");
    _ZdlPv_nid_postfix(stream);
}

void CheckCompleteDestructorKeepsBorrowedFile(const std::filesystem::path& directory) {
    const auto path = directory / "borrow.txt";
    FilePrintStream* stream = Open("borrow.txt");
    FileStream* file = stream->file;
    stream->adoptionMode = Jsc::WtfAbi::FilePrintStreamBorrow;
    Print(AsStream(stream), "before;");
    _ZN3WTF15FilePrintStreamD1Ev(stream);
    Require(fputs_nid_postfix("after", file) >= 0, "borrowed FILE stays open");
    Require(fclose_nid_postfix(file) == 0, "borrowed FILE closes once");
    Require(ReadAll(path) == "before;after", "borrowed FILE output");
    _ZdlPv_nid_postfix(stream);
}

void CheckOpenFailure() {
    const std::string path = GuestPath("missing/file.txt");
    FilePrintStreamPtr result{reinterpret_cast<FilePrintStream*>(1)};
    Require(_ZN3WTF15FilePrintStream4openEPKcS2_(&result, path.c_str(), "w") == &result, "failed open returns its result slot");
    Require(result.pointer == nullptr, "failed fopen gives a null stream");
}

void CheckSetDataFile(const std::filesystem::path& directory) {
    const auto path = directory / "installed.txt";
    FilePrintStream* installed = Open("installed.txt");
    PrintStreamPtr owner{AsStream(installed)};
    _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(&owner);
    Require(owner.pointer == nullptr, "setDataFile takes ownership");
    PrintStream& stream = _ZN3WTF8dataFileEv();
    Require(&stream == AsStream(installed), "dataFile returns the installed stream");
    Print(&stream, "routed %d", 7);
    Slot<Flush>(&stream, Jsc::WtfAbi::PrintStreamFlush)(&stream);
    Require(ReadAll(path) == "routed 7", "dataFile output reaches the installed stream");

    FilePrintStream* second = Open("second.txt");
    PrintStreamPtr secondOwner{AsStream(second)};
    bool threw = false;
    try {
        _ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE(&secondOwner);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "a second setDataFile fails the release assertion");
    Require(secondOwner.pointer == AsStream(second), "a rejected setDataFile keeps ownership with the caller");
    Require(&_ZN3WTF8dataFileEv() == AsStream(installed), "a rejected setDataFile keeps the installed stream");
    _ZN3WTF15FilePrintStreamD0Ev(second);
}

} // namespace

int main() {
    static std::array<void*, 10> defaultHeap{};
    ApplicationHeapRegister_nid_no_patch(defaultHeap.data());
    const auto directory = std::filesystem::temp_directory_path() /
        ("anyps5-wtf-data-log-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "temporary directory");
    AddPathAlias_nid_no_patch("/wtf-data-log", directory.string().c_str());
    CheckDefaultSink();
    CheckLockExcludesOtherThreads();
    CheckOpenWriteAndDelete(directory);
    CheckBaseDestructorClosesAdoptedFile(directory);
    CheckCompleteDestructorKeepsBorrowedFile(directory);
    CheckOpenFailure();
    CheckSetDataFile(directory);
    Slot<Flush>(&_ZN3WTF8dataFileEv(), Jsc::WtfAbi::PrintStreamFlush)(&_ZN3WTF8dataFileEv());
    return EXIT_SUCCESS;
}

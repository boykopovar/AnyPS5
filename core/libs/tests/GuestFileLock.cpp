#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <barrier>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <set>
#include <string>
#include <thread>
#include <windows.h>

class FileStream;

static void Require(bool condition) {
    if (!condition) std::abort();
}

static int Duplicate(int source, int kind, int target = 20) {
    if (kind == 0) return dup_nid_postfix(source);
    if (kind == 1) return dup2_nid_postfix(source, target);
    return fcntl_nid_postfix(source, 0, target);
}

static bool Acquires(int descriptor, int mode) {
    const int result = flock_nid_postfix(descriptor, mode | 4);
    if (result == 0) {
        Require(flock_nid_postfix(descriptor, 8) == 0);
        return true;
    }
    Require(result == -1 && *__error_nid_postfix() == 35);
    return false;
}

struct Fixture {
    static inline unsigned serial = 0;
    std::filesystem::path native = std::filesystem::temp_directory_path() / ("anyps5-flock-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(serial++) + ".bin");
    std::string path = native.string();
    std::set<int> descriptors;
    int source = Open(true);
    int other = Open(false);

    int Open(bool create) {
        const int descriptor = open_nid_postfix(path.c_str(), create ? 0x602 : 2, 0644);
        Require(descriptor >= 0);
        descriptors.insert(descriptor);
        return descriptor;
    }
    int Copy(int kind, int from = -1, int target = 20) {
        const int descriptor = Duplicate(from < 0 ? source : from, kind, target);
        Require(descriptor >= 0);
        descriptors.insert(descriptor);
        return descriptor;
    }
    void Close(int descriptor, bool kernel = false) {
        Require((kernel ? sceKernelClose(descriptor) : close_nid_postfix(descriptor)) == 0);
        descriptors.erase(descriptor);
    }
    ~Fixture() {
        for (const int descriptor : descriptors) Require(close_nid_postfix(descriptor) == 0);
        Require(std::filesystem::remove(native));
    }
};

static void Duplicates() {
    for (int kind = 0; kind != 3; ++kind) {
        for (bool before : {false, true}) {
            for (int mode : {1, 2}) {
                Fixture fixture;
                int copy = before ? fixture.Copy(kind) : -1;
                Require(flock_nid_postfix(fixture.source, mode | 4) == 0);
                if (!before) copy = fixture.Copy(kind);
                Require(flock_nid_postfix(copy, mode | 4) == 0);
                Require(flock_nid_postfix(fixture.source, mode | 4) == 0);
                Require(!Acquires(fixture.other, 2));
                Require(flock_nid_postfix(copy, 8) == 0);
                Require(Acquires(fixture.other, 2));
                Require(flock_nid_postfix(fixture.source, 8) == 0);
            }
        }
    }
}

static void Lifetime() {
    for (int kind = 0; kind != 3; ++kind) {
        for (bool kernel : {false, true}) {
            Fixture fixture;
            const int copy = fixture.Copy(kind);
            Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
            const int chain = fixture.Copy(0, copy);
            fixture.Close(fixture.source, kernel);
            fixture.Close(copy, !kernel);
            Require(!Acquires(fixture.other, 2));
            Require(flock_nid_postfix(chain, 8) == 0);
            Require(Acquires(fixture.other, 2));
            Require(flock_nid_postfix(chain, 2 | 4) == 0);
            fixture.Close(chain, kernel);
            Require(Acquires(fixture.other, 2));
        }
    }
    {
        Fixture fixture;
        const int copy = fixture.Copy(0);
        Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
        Require(dup2_nid_postfix(copy, fixture.source) == fixture.source);
        Require(dup2_nid_postfix(fixture.source, fixture.source) == fixture.source);
        Require(!Acquires(fixture.other, 2));
        Require(flock_nid_postfix(fixture.source, 8) == 0);
        Require(Acquires(fixture.other, 2));
    }
    {
        Fixture first;
        Fixture second;
        const int copy = second.Copy(0);
        Require(flock_nid_postfix(second.source, 2 | 4) == 0);
        Require(dup2_nid_postfix(first.source, second.source) == second.source);
        Require(!Acquires(second.other, 2));
        Require(flock_nid_postfix(copy, 8) == 0);
        Require(Acquires(second.other, 2));
        Require(flock_nid_postfix(second.source, 2 | 4) == 0);
        Require(!Acquires(first.other, 2));
        Require(flock_nid_postfix(first.source, 8) == 0);
        Require(Acquires(first.other, 2));
    }
    {
        Fixture fixture;
        const int previous = fixture.source;
        Require(flock_nid_postfix(previous, 2 | 4) == 0);
        fixture.Close(previous);
        const int recycled = fixture.Open(false);
        Require(recycled == previous && Acquires(recycled, 2));
        Require(Acquires(fixture.other, 2));
    }
}

static void Conversions() {
    for (int kind = 0; kind != 3; ++kind) {
        Fixture fixture;
        const int copy = fixture.Copy(kind);
        for (int repeat = 0; repeat != 20; ++repeat) {
            Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
            Require(flock_nid_postfix(copy, 1 | 4) == 0);
            Require(Acquires(fixture.other, 1) && !Acquires(fixture.other, 2));
            Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
            Require(!Acquires(fixture.other, 1));
            Require(flock_nid_postfix(copy, 8) == 0 && Acquires(fixture.other, 2));
        }
        Require(flock_nid_postfix(fixture.source, 1 | 4) == 0);
        Require(flock_nid_postfix(fixture.other, 1 | 4) == 0);
        Require(flock_nid_postfix(copy, 2 | 4) == -1 && *__error_nid_postfix() == 35);
        Require(flock_nid_postfix(fixture.other, 8) == 0);
        Require(Acquires(fixture.other, 2));
        Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
        char byte = 'x';
        Require(sceKernelPwrite(fixture.other, &byte, 1, 0) == 1 && sceKernelWrite(fixture.other, "yz", 2) == 2);
        Require(sceKernelPread(fixture.other, &byte, 1, 1) == 1 && byte == 'z');
        Require(flock_nid_postfix(fixture.other, 1 | 4) == -1 && *__error_nid_postfix() == 35);
        Require(flock_nid_postfix(fixture.source, 8) == 0);
    }
}

static void Threads() {
    for (int kind = 0; kind != 3; ++kind) {
        Fixture fixture;
        Require(flock_nid_postfix(fixture.source, 2 | 4) == 0);
        const int copy = fixture.Copy(kind);
        auto repeated = std::async(std::launch::async, [&] { return flock_nid_postfix(copy, 2); });
        Require(repeated.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        Require(repeated.get() == 0 && flock_nid_postfix(copy, 8) == 0);
    }
    for (bool duplicate : {false, true}) {
        Fixture fixture;
        const int copy = duplicate ? fixture.Copy(0) : fixture.source;
        std::barrier gate(3);
        int results[2] = {-2, -2};
        std::thread first([&] { gate.arrive_and_wait(); results[0] = flock_nid_postfix(fixture.source, 1 | 4); });
        std::thread second([&] { gate.arrive_and_wait(); results[1] = flock_nid_postfix(copy, 1 | 4); });
        gate.arrive_and_wait();
        first.join();
        second.join();
        Require(results[0] == 0 && results[1] == 0);
        Require(flock_nid_postfix(copy, 8) == 0 && Acquires(fixture.other, 2));
    }
    Fixture first;
    Fixture unrelated;
    Require(flock_nid_postfix(first.source, 2 | 4) == 0);
    std::promise<void> entered;
    auto entry = entered.get_future();
    auto waiting = std::async(std::launch::async, [&] {
        entered.set_value();
        return flock_nid_postfix(first.other, 2);
    });
    entry.wait();
    Require(waiting.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout);
    Require(Acquires(unrelated.source, 2));
    Require(flock_nid_postfix(first.source, 8) == 0);
    Require(waiting.wait_for(std::chrono::seconds(2)) == std::future_status::ready && waiting.get() == 0);
    Require(flock_nid_postfix(first.other, 8) == 0);
}

static void Streams() {
    Fixture first;
    Fixture second;
    auto* stream = fopen_nid_postfix(first.path.c_str(), "r+b");
    Require(stream != nullptr);
    const int descriptor = fileno_nid_postfix(stream);
    Require(flock_nid_postfix(descriptor, 2 | 4) == 0);
    Require(fclose_nid_postfix(stream) == 0 && Acquires(first.other, 2));
    for (const auto* fixture : {&second, &first}) {
        stream = fopen_nid_postfix(fixture->path.c_str(), "r+b");
        Require(stream != nullptr && fileno_nid_postfix(stream) == descriptor);
        Require(flock_nid_postfix(descriptor, 2 | 4) == 0 && !Acquires(fixture->other, 2));
        Require(fclose_nid_postfix(stream) == 0);
    }
    const int recycled = first.Open(false);
    Require(recycled == descriptor && flock_nid_postfix(recycled, 2 | 4) == 0);
    Require(!Acquires(first.other, 2));
    const int copy = first.Copy(0, recycled);
    stream = fdopen_nid_postfix(recycled, "r+");
    Require(stream != nullptr && !Acquires(first.other, 2));
    Require(fclose_nid_postfix(stream) == 0);
    first.descriptors.erase(recycled);
    Require(!Acquires(first.other, 2));
    Require(flock_nid_postfix(copy, 8) == 0 && Acquires(first.other, 2));
    stream = fopen_nid_postfix(first.path.c_str(), "r+b");
    Require(stream != nullptr);
    const int reopened = fileno_nid_postfix(stream);
    Require(flock_nid_postfix(reopened, 2 | 4) == 0);
    const int oldCopy = first.Copy(0, reopened);
    Require(freopen_nid_postfix(second.path.c_str(), "r+b", stream) == stream);
    Require(!Acquires(first.other, 2));
    Require(flock_nid_postfix(oldCopy, 8) == 0 && Acquires(first.other, 2));
    Require(flock_nid_postfix(fileno_nid_postfix(stream), 2 | 4) == 0 && !Acquires(second.other, 2));
    const auto missing = first.path + ".missing";
    Require(freopen_nid_postfix(missing.c_str(), "r+b", stream) == nullptr);
    Require(Acquires(second.other, 2));
    stream = fopen_nid_postfix(second.path.c_str(), "r+b");
    Require(stream != nullptr);
    Require(flock_nid_postfix(fileno_nid_postfix(stream), 2 | 4) == 0 && !Acquires(second.other, 2));
    Require(fclose_nid_postfix(stream) == 0);
}

static int ChildTry(const Fixture (&fixtures)[6], int mode) {
    wchar_t executable[32768];
    Require(::GetModuleFileNameW(nullptr, executable, 32768) != 0);
    std::wstring command = L"\"" + std::wstring(executable) + L"\" child " + std::to_wstring(mode | 4);
    for (const Fixture& fixture : fixtures) command += L" \"" + fixture.native.filename().wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Require(::CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != 0);
    ::CloseHandle(process.hThread);
    if (::WaitForSingleObject(process.hProcess, 30000) != WAIT_OBJECT_0) {
        ::TerminateProcess(process.hProcess, 99);
        std::abort();
    }
    DWORD result = 99;
    Require(::GetExitCodeProcess(process.hProcess, &result) != 0);
    ::CloseHandle(process.hProcess);
    return static_cast<int>(result);
}

static void Processes() {
    Fixture fixtures[6];
    int copies[6];
    int shared = 0;
    for (int index = 0; index != 6; ++index) {
        const int mode = index % 2 == 0 ? 1 : 2;
        if (mode == 1) shared |= 1 << index;
        Require(flock_nid_postfix(fixtures[index].source, mode | 4) == 0);
        copies[index] = fixtures[index].Copy(index / 2, -1, 100 + index);
    }
    Require(ChildTry(fixtures, 2) == 0x3f);
    Require(ChildTry(fixtures, 1) == (0x3f & ~shared));
    for (Fixture& fixture : fixtures) fixture.Close(fixture.source);
    Require(ChildTry(fixtures, 2) == 0x3f);
    for (const int copy : copies) Require(flock_nid_postfix(copy, 8) == 0);
    Require(ChildTry(fixtures, 2) == 0);
}

static void Directories() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / ("anyps5-flock-dir-" + std::to_string(::GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const std::string path = directory.string();
    const int descriptor = open_nid_postfix(path.c_str(), 0x20000, 0);
    Require(descriptor >= 0);
    Require(flock_nid_postfix(descriptor, 1 | 4) == 0);
    Require(flock_nid_postfix(descriptor, 2 | 4) == 0);
    Require(flock_nid_postfix(descriptor, 2) == 0);
    Require(flock_nid_postfix(descriptor, 8) == 0);
    Require(flock_nid_postfix(descriptor, 0) == -1 && *__error_nid_postfix() == 9);
    Require(flock_nid_postfix(descriptor, 4) == -1 && *__error_nid_postfix() == 9);
    Require(close_nid_postfix(descriptor) == 0);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}

int main(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "child") {
        int busy = 0;
        for (int index = 3; index < argc; ++index) {
            const std::string path = (std::filesystem::temp_directory_path() / argv[index]).string();
            const int descriptor = open_nid_postfix(path.c_str(), 2, 0);
            Require(descriptor >= 0);
            if (flock_nid_postfix(descriptor, std::atoi(argv[2])) == 0) {
                Require(flock_nid_postfix(descriptor, 8) == 0);
            } else {
                Require(*__error_nid_postfix() == 35);
                busy |= 1 << (index - 3);
            }
            Require(close_nid_postfix(descriptor) == 0);
        }
        return busy;
    }
    Require(argc == 2);
    const std::string mode = argv[1];
    if (mode == "duplicate") Duplicates();
    else if (mode == "lifetime") Lifetime();
    else if (mode == "conversion") Conversions();
    else if (mode == "threads") Threads();
    else if (mode == "streams") Streams();
    else if (mode == "process") Processes();
    else if (mode == "directory") Directories();
    else std::abort();
}

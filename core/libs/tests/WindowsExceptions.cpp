#include <stdexcept>
#include <string>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <mutex>
#include <thread>
#include <cstddef>
#include <typeinfo>
#include <windows.h>
#include "../prx/libc/include/general/VabiMacros.hpp"

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix[];
}

static void TestTypeInfoVtables() {
    struct TypeRecord { const void* vtable; const char* name; };
    struct SingleRecord { TypeRecord type; const TypeRecord* base; };
    struct BaseRecord { const TypeRecord* type; std::ptrdiff_t flags; };
    struct MultipleRecord { TypeRecord type; unsigned flags; unsigned count; BaseRecord bases[2]; };
    const auto* classTable = _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix;
    const auto* singleTable = _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix;
    const auto* multipleTable = _ZTVN10__cxxabiv121__vmi_class_type_infoE_nid_postfix;
    const unsigned char* tables[] {classTable, singleTable, multipleTable};
    const char* names[] {"N10__cxxabiv117__class_type_infoE", "N10__cxxabiv120__si_class_type_infoE", "N10__cxxabiv121__vmi_class_type_infoE"};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::type_info* category;
        std::memcpy(&category, tables[i] + sizeof(void*), sizeof(category));
        if (std::strcmp(category->name(), names[i])) throw std::runtime_error("incorrect RTTI category");
    }
    TypeRecord base {classTable + 2 * sizeof(void*), "4Base"};
    TypeRecord other {classTable + 2 * sizeof(void*), "5Other"};
    SingleRecord single {{singleTable + 2 * sizeof(void*), "6Single"}, &base};
    MultipleRecord multiple {{multipleTable + 2 * sizeof(void*), "8Multiple"}, 0, 2, {{&other, 2}, {&base, (16 << 8) | 2}}};
    using CatchType = bool (APS5_VABI *)(const void*, const void*, void**, unsigned);
    using UpcastType = bool (APS5_VABI *)(const void*, const void*, void**);
    CatchType catchType;
    UpcastType upcastType;
    std::memcpy(&catchType, classTable + 6 * sizeof(void*), sizeof(catchType));
    std::memcpy(&upcastType, multipleTable + 7 * sizeof(void*), sizeof(upcastType));
    std::array<unsigned char, 32> storage {};
    void* object = storage.data();
    if (!catchType(&base, &single, &object, 0) || object != storage.data()) throw std::runtime_error("single RTTI catch failed");
    object = storage.data();
    if (!catchType(&base, &multiple, &object, 0) || object != storage.data() + 16) throw std::runtime_error("multiple RTTI catch failed");
    object = storage.data();
    if (!upcastType(&multiple, &base, &object) || object != storage.data() + 16) throw std::runtime_error("multiple RTTI upcast failed");
    object = storage.data();
    if (catchType(&other, &single, &object, 0)) throw std::runtime_error("unrelated RTTI catch succeeded");
    TypeRecord shared {classTable + 2 * sizeof(void*), "6Shared"};
    SingleRecord publicPath {{singleTable + 2 * sizeof(void*), "10PublicPath"}, &shared};
    SingleRecord privatePath {{singleTable + 2 * sizeof(void*), "11PrivatePath"}, &shared};
    MultipleRecord mixed {{multipleTable + 2 * sizeof(void*), "5Mixed"}, 1, 2, {{&publicPath.type, 2}, {&privatePath.type, 16 << 8}}};
    MultipleRecord hidden {{multipleTable + 2 * sizeof(void*), "6Hidden"}, 0, 1, {{&privatePath.type, 0}, {}}};
    object = storage.data();
    if (catchType(&shared, &mixed, &object, 0) || object != storage.data()) throw std::runtime_error("RTTI catch of a base repeated through a private path succeeded");
    if (upcastType(&mixed, &shared, &object) || object != storage.data()) throw std::runtime_error("RTTI upcast to a base repeated through a private path succeeded");
    if (!catchType(&publicPath, &mixed, &object, 0) || object != storage.data()) throw std::runtime_error("RTTI catch of the public path failed");
    object = storage.data();
    if (catchType(&shared, &hidden, &object, 0)) throw std::runtime_error("RTTI catch of a privately inherited base succeeded");
}

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char*);

static void RequireHandle(bool value) {
    if (!value) throw std::runtime_error("child process setup failed");
}

[[gnu::noinline, noreturn]] static void ThrowUncaughtProbe() {
    throw std::runtime_error("unhandled site probe");
}

static int RunThrowChild() {
    std::fprintf(stderr, "probe %p\n", reinterpret_cast<const void*>(&ThrowUncaughtProbe));
    std::fflush(stderr);
    ThrowUncaughtProbe();
    return 100;
}

static std::string RunChild(const wchar_t* argument, DWORD& exitCode) {
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    RequireHandle(CreatePipe(&readPipe, &writePipe, &attributes, 0) != 0);
    RequireHandle(SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0) != 0);
    std::array<wchar_t, 32768> image{};
    RequireHandle(GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size())) != 0);
    std::array<wchar_t, 32768> command{};
    const std::size_t imageLength = std::wcslen(image.data());
    const std::size_t argumentLength = std::wcslen(argument);
    RequireHandle(1 + imageLength + argumentLength + 1 < command.size());
    command[0] = L'"';
    std::memcpy(command.data() + 1, image.data(), imageLength * sizeof(wchar_t));
    std::memcpy(command.data() + 1 + imageLength, argument, (argumentLength + 1) * sizeof(wchar_t));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION child{};
    RequireHandle(CreateProcessW(image.data(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child) != 0);
    CloseHandle(writePipe);
    CloseHandle(child.hThread);
    std::array<char, 65536> output{};
    std::size_t outputLength = 0;
    DWORD chunk = 0;
    while (outputLength < output.size() && ReadFile(readPipe, output.data() + outputLength, static_cast<DWORD>(output.size() - outputLength), &chunk, nullptr) != 0 && chunk != 0) {
        outputLength += chunk;
    }
    CloseHandle(readPipe);
    RequireHandle(WaitForSingleObject(child.hProcess, 20000) == WAIT_OBJECT_0);
    RequireHandle(GetExitCodeProcess(child.hProcess, &exitCode) != 0);
    CloseHandle(child.hProcess);
    output[outputLength < output.size() ? outputLength : output.size() - 1] = 0;
    return std::string(output.data());
}

static void testUnhandledThrowSite() {
    DWORD exitCode = 0;
    const std::string output = RunChild(L"\" throw-uncaught", exitCode);
    RequireHandle(exitCode == 0xC0000409);
    const char* probeMarker = std::strstr(output.c_str(), "probe ");
    RequireHandle(probeMarker != nullptr);
    const auto probe = static_cast<std::uintptr_t>(std::strtoull(probeMarker + 6, nullptr, 16));
    RequireHandle(probe != 0);
    const char* marker = std::strstr(output.c_str(), "thrown from ");
    RequireHandle(marker != nullptr);
    const auto logged = static_cast<std::uintptr_t>(std::strtoull(marker + 13, nullptr, 16));
    RequireHandle(logged != 0);
    if (logged <= probe || logged - probe >= 0x1000) throw std::runtime_error("unhandled exception named the wrapper instead of the throw site");
}

[[gnu::noinline, noreturn]] static void GuestAbortProbe() {
    __asm__ volatile("int $0x45");
    __builtin_unreachable();
}

static int RunAbortChild() {
    std::fprintf(stderr, "abort probe\n");
    std::fflush(stderr);
    GuestAbortProbe();
}

static void testGuestAbortReport() {
    DWORD exitCode = 0;
    const std::string output = RunChild(L"\" guest-abort", exitCode);
    char code[16];
    std::snprintf(code, sizeof(code), "0x%08lx", static_cast<unsigned long>(exitCode));
    if (exitCode != 0xC0000005 || output.find("guest abort() (int 0x45)") == std::string::npos)
        throw std::runtime_error(std::string("guest abort child exited with ") + code + " and printed:\n" + output);
}

[[gnu::noinline, noreturn]] static void GuestAssertProbe() {
    __asm__ volatile("int $0x41");
    __builtin_unreachable();
}

static int RunAssertChild() {
    std::fprintf(stderr, "assert probe\n");
    std::fflush(stderr);
    GuestAssertProbe();
}

static void testGuestAssertReport() {
    DWORD exitCode = 0;
    const std::string output = RunChild(L"\" guest-assert", exitCode);
    char code[16];
    std::snprintf(code, sizeof(code), "0x%08lx", static_cast<unsigned long>(exitCode));
    if (exitCode != 0xC0000005 || output.find("guest assert/break (int 0x41)") == std::string::npos)
        throw std::runtime_error(std::string("guest assert child exited with ") + code + " and printed:\n" + output);
}

static int destroyed;
struct Guard { ~Guard() { ++destroyed; } };
static void ThrowNested() {
    Guard guard;
    throw std::runtime_error("native own unwind");
}
static void Rethrow() {
    Guard guard;
    try { ThrowNested(); }
    catch (const std::runtime_error&) { throw; }
}

class TrackedError : public std::runtime_error {
public:
    explicit TrackedError(std::atomic<int>* count) : std::runtime_error("retained error"), count(count) {}
    ~TrackedError() override { ++*count; }
private:
    std::atomic<int>* count;
};

static void testExceptionPointer() {
    std::atomic<int> count{0};
    std::atomic<int> caught{0};
    std::exception_ptr retained;
    const TrackedError* original = nullptr;
    try {
        throw TrackedError(&count);
    } catch (const TrackedError& error) {
        original = &error;
        retained = std::current_exception();
    }
    if (!retained || count != 0) throw std::runtime_error("exception was not retained");
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([retained, original, &caught] {
            try {
                std::rethrow_exception(retained);
            } catch (const TrackedError& error) {
                if (&error == original && std::strcmp(error.what(), "retained error") == 0) ++caught;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    if (caught != 4 || count != 0) throw std::runtime_error("cross-thread rethrow failed");
    auto copy = retained;
    retained = nullptr;
    if (count != 0) throw std::runtime_error("exception copy lost ownership");
    copy = nullptr;
    if (count != 1) throw std::runtime_error("exception destroyed an incorrect number of times");
    if (std::current_exception()) throw std::runtime_error("stale current exception");
}

static void testFutureException() {
    std::atomic<int> count{0};
    {
        std::promise<void> promise;
        auto future = promise.get_future();
        try {
            throw TrackedError(&count);
        } catch (const TrackedError&) {
            promise.set_exception(std::current_exception());
        }
        try {
            future.get();
            throw std::runtime_error("future did not rethrow");
        } catch (const TrackedError& error) {
            if (std::strcmp(error.what(), "retained error")) throw std::runtime_error("future rethrew another error");
        }
    }
    if (count != 1) throw std::runtime_error("future result destroyed its exception an incorrect number of times");
}

static void testMadeExceptionPointer() {
    std::atomic<int> count{0};
    {
        auto made = std::make_exception_ptr(TrackedError(&count));
        try {
            std::rethrow_exception(made);
        } catch (const TrackedError& error) {
            if (std::strcmp(error.what(), "retained error")) throw std::runtime_error("made exception lost its message");
        }
    }
    if (count != 2) throw std::runtime_error("made exception destroyed an incorrect number of times");
}

struct UnwindProbe {
    int& seen;
    ~UnwindProbe() { seen = std::uncaught_exceptions(); }
};

static void testUncaughtExceptions() {
    if (std::uncaught_exceptions() != 0) throw std::runtime_error("uncaught exception outside unwinding");
    int seen = -1;
    try {
        UnwindProbe probe{seen};
        throw std::runtime_error("probe");
    } catch (const std::runtime_error&) {
    }
    if (seen != 1) throw std::runtime_error("unwinding did not count its exception");
}

static int guestFrameDestroyed;
struct GuestFrameGuard { ~GuestFrameGuard() { ++guestFrameDestroyed; } };
static void ThrowThroughGuestFrames(int depth) {
    GuestFrameGuard guard;
    if (depth) { ThrowThroughGuestFrames(depth - 1); return; }
    throw std::runtime_error("through guest frames");
}

static void testUnwindThroughGuestFrames() {
    for (int depth : {0, 1, 7, 64}) {
        guestFrameDestroyed = 0;
        try {
            ThrowThroughGuestFrames(depth);
            throw std::logic_error("guest frame did not propagate");
        } catch (const std::runtime_error& error) {
            if (std::strcmp(error.what(), "through guest frames")) throw;
        }
        if (guestFrameDestroyed != depth + 1) throw std::runtime_error("unwinding through guest frames skipped a destructor");
    }
}

static int onceDestroyed;
struct OnceGuard { ~OnceGuard() { ++onceDestroyed; } };
static void ThrowThroughOnce(int depth) {
    OnceGuard guard;
    if (depth) { ThrowThroughOnce(depth - 1); return; }
    std::once_flag flag;
    std::call_once(flag, [] { throw std::runtime_error("through runtime frames"); });
}

static void testUnwindThroughRuntimeModules() {
    for (int depth : {0, 1, 7, 64}) {
        onceDestroyed = 0;
        try {
            ThrowThroughOnce(depth);
            throw std::logic_error("call_once did not propagate");
        } catch (const std::runtime_error& error) {
            if (std::strcmp(error.what(), "through runtime frames")) throw;
        }
        if (onceDestroyed != depth + 1) throw std::runtime_error("unwinding through runtime DLL frames skipped a destructor");
    }
}

int main(int argc, char** argv) {
    if (argc > 1) {
        if (std::strcmp(argv[1], "throw-uncaught") == 0) return RunThrowChild();
        if (std::strcmp(argv[1], "guest-abort") == 0) return RunAbortChild();
        if (std::strcmp(argv[1], "guest-assert") == 0) return RunAssertChild();
        return 100;
    }
    TestTypeInfoVtables();
    testUnhandledThrowSite();
    testGuestAbortReport();
    testGuestAssertReport();
    testExceptionPointer();
    testFutureException();
    testMadeExceptionPointer();
    testUncaughtExceptions();
    testUnwindThroughGuestFrames();
    testUnwindThroughRuntimeModules();
    try {
        Rethrow();
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::strcmp(error.what(), "native own unwind") || destroyed != 2) return 2;
    }
    try { NotImplemented_nid_no_patch("cross DLL"); }
    catch (const std::runtime_error& error) {
        if (std::strcmp(error.what(), "cross DLL not implemented")) return 3;
        try {
            Guard guard;
            throw;
        } catch (const std::logic_error&) {
            return 4;
        } catch (const std::exception& rethrown) {
            if (&rethrown != &error || destroyed != 3) return 5;
        }
        try {
            Guard guard;
            throw 42;
        } catch (const std::exception&) {
            return 6;
        } catch (...) {
            if (destroyed != 4) return 7;
            try {
                throw;
            } catch (int value) {
                if (value != 42) return 8;
            }
        }
        std::puts("Own Windows exceptions: typed catch, base catch, catch-all, rethrow, destructors, cross DLL passed");
        return 0;
    }
    return 9;
}

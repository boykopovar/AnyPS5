#include "prx/libkernel/include/Api.hpp"
#define WIN32_LEAN_AND_MEAN
#include "GuestDeferredModule.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>

static void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "%s\n", what);
        std::exit(1);
    }
}

static void* Handle(int id) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)); }

static int Open(const std::string& path, const GuestDeferredModuleArgs* args = nullptr, bool initialized = true) {
    int result = -1;
    const int id = sceKernelLoadStartModule(path.c_str(), args ? sizeof(*args) : 0, args, 0, nullptr, &result);
    Require(id >= 0, "load deferred module");
    Require(result == (initialized ? GuestDeferredModuleResult : 0), "module start result");
    return id;
}

static void Close(int id) { Require(sceKernelStopUnloadModule(id, 0, nullptr, 0, nullptr, nullptr) == 0, "close module"); }

static void Counts(int id, int expectedStarts, int expectedConstructors) {
    using Count = int (APS5_VABI*)();
    const auto starts = reinterpret_cast<Count>(dlsym_nid_postfix(Handle(id), "DeferredStarts_nid_no_patch"));
    const auto constructors = reinterpret_cast<Count>(dlsym_nid_postfix(Handle(id), "DeferredConstructors_nid_no_patch"));
    Require(starts && constructors, "find fixture counters");
    if (starts() != expectedStarts || constructors() != expectedConstructors) {
        std::fprintf(stderr, "starts=%d constructors=%d, expected=%d/%d\n", starts(), constructors(), expectedStarts, expectedConstructors);
        std::exit(1);
    }
}

static HMODULE Native(int id) {
    const auto symbol = dlsym_nid_postfix(Handle(id), "DeferredStarts_nid_no_patch");
    HMODULE native = nullptr;
    Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(symbol), &native), "native image handle");
    return native;
}

static void MetadataProtected(int id) {
    const auto table = GetProcAddress(Native(id), "__aps5_guest_initialize");
    MEMORY_BASIC_INFORMATION information{};
    Require(table && VirtualQuery(reinterpret_cast<const void*>(table), &information, sizeof(information)), "query init metadata");
    Require(information.Protect == PAGE_READONLY, "init metadata protection restored");
}

static void APS5_VABI Reenter(const char* path) {
    void* nested = dlopen_nid_postfix(path, 2);
    Require(nested != nullptr, "recursive module load");
    Require(dlclose_nid_postfix(nested) == 0, "recursive module close");
}

int main(int argc, char** argv) {
    Require(argc == 3, "fixture and mode required");
    const auto file = std::filesystem::absolute(argv[1]);
    const auto path = file.string();
    const std::string mode = argv[2];
    if (mode == "reload") {
        HMODULE firstBase = nullptr;
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            const int first = Open(path);
            Counts(first, 1, 1);
            const auto native = Native(first);
            if (cycle == 0) firstBase = native;
            Require(native == firstBase, "reload reuses the fixture base");
            MetadataProtected(first);
            const int second = Open(path, nullptr, false);
            Counts(second, 1, 1);
            Close(first);
            Require(GetModuleHandleW(file.c_str()) != nullptr, "second reference retains the module");
            Counts(second, 1, 1);
            Close(second);
            Require(GetModuleHandleW(file.c_str()) == nullptr, "last close unloads the module");
        }
    } else if (mode == "pinned") {
        HMODULE pin = LoadLibraryW(file.c_str());
        Require(pin != nullptr, "native pin");
        const int first = Open(path);
        Counts(first, 1, 1);
        Close(first);
        const int second = Open(path, nullptr, false);
        Counts(second, 1, 1);
        MetadataProtected(second);
        Close(second);
        Require(FreeLibrary(pin) && GetModuleHandleW(file.c_str()) == nullptr, "release native pin");
        const int reloaded = Open(path);
        Counts(reloaded, 1, 1);
        Close(reloaded);
    } else if (mode == "parallel") {
        GuestDeferredModuleArgs args;
        args.Mode = 1;
        args.Entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        args.Release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Require(args.Entered && args.Release, "create initializer gates");
        auto first = std::async(std::launch::async, [&] { return Open(path, &args); });
        Require(WaitForSingleObject(args.Entered, 3000) == WAIT_OBJECT_0, "first initializer entered");
        std::promise<void> attempting;
        auto started = attempting.get_future();
        auto second = std::async(std::launch::async, [&] {
            attempting.set_value();
            return Open(path, nullptr, false);
        });
        started.wait();
        const bool returnedEarly = second.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
        Require(SetEvent(args.Release), "release initializer");
        const int one = first.get();
        const int two = second.get();
        Require(!returnedEarly, "concurrent load waits for initialization");
        Counts(one, 1, 1);
        Counts(two, 1, 1);
        MetadataProtected(one);
        Close(one);
        Close(two);
        CloseHandle(args.Entered);
        CloseHandle(args.Release);
    } else if (mode == "reentrant") {
        GuestDeferredModuleArgs args;
        args.Mode = 2;
        args.Reenter = Reenter;
        args.Path = path.c_str();
        const int id = Open(path, &args);
        Counts(id, 1, 1);
        MetadataProtected(id);
        Close(id);
    } else Require(false, "unknown fixture mode");
}

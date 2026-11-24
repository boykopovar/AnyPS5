#include "prx/libSceFreeTypeOt/include/ModuleService.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using namespace FreeType;

std::vector<GuestServiceModuleRec*> calls;
const char* expectedId = nullptr;
decltype(&ft_module_get_service_nid_postfix) lookupService = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType module service: %s\n", message);
    std::exit(1);
}

struct Module {
    GuestServiceModuleRec record{};
    GuestModuleClass clazz{};
    const char* provided = nullptr;
    bool providesNull = false;
    GuestServiceLibraryRec* replacement = nullptr;
    GuestServiceModuleRec** replaceSlot = nullptr;
    GuestServiceModuleRec* replaceWith = nullptr;
    int service = 0;

    Module();
};

void* APS5_VABI Request_nid_no_patch(GuestServiceModuleRec* record, const char* id) {
    Require(id == expectedId, "service ID pointer was changed");
    calls.push_back(record);
    auto* module = reinterpret_cast<Module*>(record);
    if (module->replacement) record->library = module->replacement;
    if (module->replaceSlot) *module->replaceSlot = module->replaceWith;
    if (!id) return module->providesNull ? &module->service : nullptr;
    return module->provided && std::strcmp(id, module->provided) == 0 ? &module->service : nullptr;
}

Module::Module() {
    record.clazz = &clazz;
    clazz.get_interface = reinterpret_cast<void*>(Request_nid_no_patch);
}

void ExpectCalls(std::initializer_list<GuestServiceModuleRec*> expected) {
    Require(calls == std::vector<GuestServiceModuleRec*>(expected), "wrong callback order or duplicate lookup");
}

void* Lookup(GuestServiceModuleRec* module, const char* id) {
    expectedId = id;
    calls.clear();
    return lookupService(module, id);
}

template <typename TCall>
void RequireThrows(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return;
    }
    Require(false, "inaccessible or malformed record was accepted");
}

void Search() {
    Module own, first, absent, last;
    GuestServiceLibraryRec library{};
    own.record.library = &library;
    library.numModules = 4;
    library.modules[0] = &first.record;
    library.modules[1] = &own.record;
    library.modules[2] = &absent.record;
    library.modules[3] = &last.record;
    first.provided = last.provided = "shared";
    own.provided = "local";
    absent.clazz.get_interface = nullptr;

    Require(Lookup(nullptr, "local") == nullptr, "null module did not return null");
    ExpectCalls({});
    Require(Lookup(&own.record, "local") == &own.service, "own service was not preferred");
    ExpectCalls({&own.record});
    Require(Lookup(&own.record, "shared") == &first.service, "first available service was not selected");
    ExpectCalls({&own.record, &first.record});
    Require(Lookup(&own.record, "missing") == nullptr, "missing service did not return null");
    ExpectCalls({&own.record, &first.record, &last.record});

    own.clazz.get_interface = nullptr;
    Require(Lookup(&own.record, "shared") == &first.service, "missing own requester prevented fallback");
    ExpectCalls({&first.record});
    own.clazz.get_interface = reinterpret_cast<void*>(Request_nid_no_patch);
    last.providesNull = true;
    Require(Lookup(&own.record, nullptr) == &last.service, "null service ID was not delegated");
    ExpectCalls({&own.record, &first.record, &last.record});

    own.record.library = nullptr;
    Require(Lookup(&own.record, "local") == &own.service, "own hit unnecessarily accessed the library");
    library.numModules = 0;
    own.record.library = &library;
    Require(Lookup(&own.record, "missing") == nullptr, "empty module list did not return null");
    ExpectCalls({&own.record});
}

void CallbackChanges() {
    Module own, first, replacement;
    GuestServiceLibraryRec initial{}, updated{};
    own.record.library = &initial;
    own.replacement = &updated;
    updated.numModules = 2;
    updated.modules[0] = &first.record;
    updated.modules[1] = &own.record;
    replacement.provided = "replacement";
    first.replaceSlot = &updated.modules[1];
    first.replaceWith = &replacement.record;
    Require(Lookup(&own.record, "replacement") == &replacement.service, "callback changes were not observed");
    ExpectCalls({&own.record, &first.record, &replacement.record});
}

void FullLibrary() {
    Module own;
    std::array<Module, 32> modules;
    GuestServiceLibraryRec library{};
    own.record.library = &library;
    library.numModules = 32;
    for (std::size_t i = 0; i < modules.size(); ++i) library.modules[i] = &modules[i].record;
    modules.back().provided = "last";
    Require(Lookup(&own.record, "last") == &modules.back().service, "last module was not searched");
    Require(calls.size() == 33 && calls.front() == &own.record, "full library callback count is wrong");
    for (std::size_t i = 0; i < modules.size(); ++i) Require(calls[i + 1] == &modules[i].record, "full library order is wrong");
}

void InvalidRecords() {
    Module own;
    GuestServiceLibraryRec library{};
    own.record.library = &library;
    own.clazz.get_interface = nullptr;
    RequireThrows([&] { Lookup(reinterpret_cast<GuestServiceModuleRec*>(1), nullptr); });
    own.record.clazz = nullptr;
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    own.record.clazz = reinterpret_cast<GuestModuleClass*>(1);
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    own.record.clazz = &own.clazz;
    own.record.library = nullptr;
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    own.record.library = reinterpret_cast<GuestServiceLibraryRec*>(1);
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    own.record.library = &library;
    library.numModules = 1;
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    library.modules[0] = reinterpret_cast<GuestServiceModuleRec*>(1);
    RequireThrows([&] { Lookup(&own.record, nullptr); });
    library.numModules = 33;
    RequireThrows([&] { Lookup(&own.record, nullptr); });
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "library path and raw/patched mode required");
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "raw") == 0, "unknown library mode");
    const char* symbol = patched ? "uSNqxrARpEQ" : "ft_module_get_service_nid_postfix";
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "cannot load library");
    lookupService = reinterpret_cast<decltype(lookupService)>(GetProcAddress(library, symbol));
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(library != nullptr, "cannot load library");
    lookupService = reinterpret_cast<decltype(lookupService)>(dlsym(library, symbol));
#endif
    Require(lookupService != nullptr, "module service export not found");
    try {
        Search();
        CallbackChanges();
        FullLibrary();
        InvalidRecords();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FreeType module service: unexpected exception: %s\n", error.what());
        return 1;
    }
    std::puts("FreeType module service checks passed");
}

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <unistd.h>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Posix spawn flags check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static std::atomic<int> observedSpawnFlags{-1};

extern "C" int posix_spawn(pid_t* process, const char* path, const posix_spawn_file_actions_t* actions,
    const posix_spawnattr_t* attributes, char* const arguments[], char* const environment[]) {
    short flags = 0;
    Check(attributes == nullptr || posix_spawnattr_getflags(attributes, &flags) == 0);
    static const auto spawn = reinterpret_cast<decltype(&posix_spawn)>(dlsym(RTLD_NEXT, "posix_spawn"));
    Check(spawn != nullptr);
    observedSpawnFlags.store(flags, std::memory_order_relaxed);
    return spawn(process, path, actions, attributes, arguments, environment);
}

int main(int count, char** arguments) {
    if (count == 2 && std::strcmp(arguments[1], "spawn-flags-child") == 0) return 43;
    std::filesystem::create_directories("app0");
    {
        std::ofstream image("app0/eboot.bin");
        image << "image";
        Check(image.good());
    }
    PosixSpawnAttributes* attributes = nullptr;
    Check(posix_spawnattr_init_nid_postfix(&attributes) == 0);
    Check(posix_spawnattr_setpgroup_nid_postfix(&attributes, 0) == 0);
    constexpr int expectedFlags[] = {0, POSIX_SPAWN_RESETIDS, POSIX_SPAWN_SETPGROUP,
        POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP};
    char program[] = "/app0/eboot.bin";
    char role[] = "spawn-flags-child";
    char* childArguments[] = {program, role, nullptr};
    for (std::int16_t flags = 0; flags < 4; ++flags) {
        Check(posix_spawnattr_setflags_nid_postfix(&attributes, flags) == 0);
        observedSpawnFlags.store(-1, std::memory_order_relaxed);
        std::int32_t process = -1;
        Check(posix_spawn_nid_postfix(&process, program, nullptr, &attributes, childArguments, nullptr) == 0);
        Check(process > 0);
        const int observed = observedSpawnFlags.load(std::memory_order_relaxed);
        if (observed != expectedFlags[flags])
            std::fprintf(stderr, "Spawn flags %d: expected host flags %d, observed %d\n", flags, expectedFlags[flags], observed);
        Check(observed == expectedFlags[flags]);
        Check(getpgid(process) == ((flags & 2) != 0 ? process : getpgrp()));
        int status = -1;
        Check(waitpid_nid_postfix(process, &status, 0) == process);
        Check((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 43);
    }
    Check(posix_spawnattr_destroy_nid_postfix(&attributes) == 0);
    Check(std::filesystem::remove("app0/eboot.bin"));
    Check(std::filesystem::remove("app0"));
}

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

int main(int count, char** arguments) {
#ifdef _WIN32
    constexpr int ChildArgument = 2;
#else
    constexpr int ChildArgument = 1;
#endif
    if (count == ChildArgument + 1 && std::strcmp(arguments[ChildArgument], "spawn-optional-pid-child") == 0) return 43;
    std::filesystem::create_directories("app0");
    {
        std::ofstream image("app0/eboot.bin");
        image << "image";
        Require(image.good());
    }
    char program[] = "/app0/eboot.bin";
    char role[] = "spawn-optional-pid-child";
    char* childArguments[] = {program, role, nullptr};
    const std::array spawnFunctions{&posix_spawn_nid_postfix, &posix_spawnp_nid_postfix};
    for (std::size_t index = 0; index < spawnFunctions.size(); ++index) {
        const auto spawn = spawnFunctions[index];
        std::fprintf(stderr, "Spawn wrapper %zu: testing a null PID output\n", index);
        Require(spawn(nullptr, program, nullptr, nullptr, childArguments, nullptr) == 0);
        int status = -1;
        Require(waitpid_nid_postfix(-1, &status, 0) > 0);
        Require((status & 0x7f) == 0 && ((status >> 8) & 0xff) == 43);
        Require(spawn(nullptr, "/app0/missing.bin", nullptr, nullptr, childArguments, nullptr) == 2);
        Require(spawn(nullptr, program, nullptr, nullptr, nullptr, nullptr) == 14);
    }
    Require(std::filesystem::remove("app0/eboot.bin"));
    Require(std::filesystem::remove("app0"));
}

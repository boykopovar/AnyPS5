#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <vector>
#endif

namespace {

constexpr const char* Started = "--started-by-load-exec";
constexpr const char* Secondary = "--secondary";

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "LoadExec secondary: %s\n", message);
        std::abort();
    }
}

std::filesystem::path Own() {
#ifdef __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size);
    Require(_NSGetExecutablePath(path.data(), &size) == 0, "locating the test executable");
    return std::filesystem::absolute(path.data());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

int StartThirdFromSecond(const std::filesystem::path& own, const std::string& extension) {
    const std::string stem = "anyps5-load-exec-secondary-" + std::to_string(::getpid()) + (extension.empty() ? "-bare" : "-suffixed");
    const auto directory = own.parent_path();
    const auto main = directory / (stem + extension);
    const auto second = directory / (stem + ".second.bin" + extension);
    const auto third = directory / (stem + ".third.bin" + extension);
    for (const auto& link : {main, second, third}) {
        std::filesystem::remove(link);
        std::filesystem::create_hard_link(own, link);
    }
    const std::string guest = "/app0/" + stem + "/third.bin";
    const pid_t child = ::fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        if (::setenv("APS5_EBOOT", main.c_str(), 1) != 0) ::_exit(72);
        ::execl(second.c_str(), second.c_str(), Secondary, guest.c_str(), static_cast<char*>(nullptr));
        ::_exit(73);
    }
    int status = 0;
    const bool waited = ::waitpid(child, &status, 0) == child;
    for (const auto& link : {main, second, third}) std::filesystem::remove(link);
    Require(waited, "waiting for the secondary executable");
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

}

int main(int argc, char** argv) {
    if (argc == 1 && std::strcmp(argv[0], Started) == 0) return 0;
    if (argc == 3 && std::strcmp(argv[1], Secondary) == 0) {
        const char* const arguments[] = {Started, nullptr};
        try {
            sceSystemServiceLoadExec(argv[2], arguments);
        } catch (const std::runtime_error& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 75;
        }
        return 76;
    }
    const auto own = Own();
    Require(StartThirdFromSecond(own, ".elf") == 0, "a secondary executable did not start another one beside a main executable with an extension");
    Require(StartThirdFromSecond(own, "") == 0, "a secondary executable did not start another one beside a main executable without an extension");
    std::puts("LoadExec secondary tests passed");
    return 0;
}

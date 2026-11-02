#include "prx/libkernel/System/include/InstanceLockHost.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static std::filesystem::path LockFile() {
    return InstanceLockPath(std::filesystem::current_path());
}

static void CheckHeldAndReleased() {
    auto other = MakeHostInstanceLockBackend();
    Require(std::filesystem::exists(LockFile()), "start-up must have created the lock file");
    Require(!other->TryLock(LockFile()), "start-up must hold the lock for the whole process");
    ReleaseHostInstanceLock();
    Require(other->TryLock(LockFile()), "ReleaseHostInstanceLock must free the lock");
}

static void CheckNothingCreated() {
    Require(!std::filesystem::exists(LockFile()), "no lock file may be created: " + LockFile().string());
}

#ifndef _WIN32
static int RunChild(const std::string& executable, const char* mode, const std::filesystem::path& errors) {
    const std::string command = std::string("ANYPS5_INSTANCE_LOCK=") + mode + " '" + executable + "' child >/dev/null 2>'" + errors.string() + "'";
    return std::system(command.c_str());
}

static std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static void CheckInvalidValue(const std::string& executable) {
    const auto errors = std::filesystem::current_path() / "child-errors.txt";
    for (const char* value : {"yes", "1", "ON", "auto"}) {
        Require(RunChild(executable, value, errors) != 0, std::string("a copy with the value '") + value + "' must stop");
        const auto message = Read(errors);
        Require(message.find(std::string("ANYPS5_INSTANCE_LOCK must be on or off, not '") + value + "'") != std::string::npos,
            std::string("the error must name the variable and quote '") + value + "': " + message);
    }
    Require(RunChild(executable, "", errors) == 0, "an empty value must be the same as unset: " + Read(errors));
}

static void CheckUnreadableTitle(const std::string& executable) {
    const auto errors = std::filesystem::current_path() / "child-errors.txt";
    Require(RunChild(executable, "off", errors) == 0, "a copy with the check off must ignore the unreadable title: " + Read(errors));
    Require(RunChild(executable, "on", errors) != 0, "a title whose ID cannot be read must stop with the check on");
    const auto message = Read(errors);
    Require(message.find(std::filesystem::current_path().string()) != std::string::npos, "the error must name the directory: " + message);
    Require(message.find("ANYPS5_INSTANCE_LOCK=off") != std::string::npos, "the error must name the variable: " + message);
    Require(!std::filesystem::exists(LockFile()), "no lock file may be created when the title ID is unreadable");
}

static void CheckSecondInstance(const std::string& executable) {
    const auto errors = std::filesystem::current_path() / "child-errors.txt";
    Require(RunChild(executable, "off", errors) == 0, "a copy with the check off must run beside the holder: " + Read(errors));
    Require(RunChild(executable, "on", errors) != 0, "a second copy with the check on must stop");
    const auto message = Read(errors);
    Require(message.find("PPSA00000") != std::string::npos, "the error must name the title ID: " + message);
    Require(message.find(std::filesystem::current_path().string()) != std::string::npos, "the error must name the directory: " + message);
    Require(message.find(LockFile().string()) != std::string::npos, "the error must name the lock file: " + message);
    Require(message.find("ANYPS5_INSTANCE_LOCK=off") != std::string::npos, "the error must name the variable: " + message);
    ReleaseHostInstanceLock();
    Require(RunChild(executable, "on", errors) == 0, "a copy must start once the holder released the lock: " + Read(errors));
}
#endif

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        if (mode == "child") return 0;
        if (mode == "held") CheckHeldAndReleased();
        else if (mode == "nothing") CheckNothingCreated();
#ifndef _WIN32
        else if (mode == "second") CheckSecondInstance(argv[0]);
        else if (mode == "invalid") CheckInvalidValue(argv[0]);
        else if (mode == "unreadable") CheckUnreadableTitle(argv[0]);
#endif
        else throw std::runtime_error("unknown mode '" + mode + "'");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

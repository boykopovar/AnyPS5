#include "prx/libkernel/System/include/InstanceLock.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

namespace {

class FakeBackend final : public IInstanceLockBackend {
public:
    explicit FakeBackend(bool acquired) : acquired(acquired) {}
    bool TryLock(const std::filesystem::path& file) override {
        requested.push_back(file);
        if (failure) throw std::runtime_error("cannot open " + file.string());
        return acquired;
    }
    std::vector<std::filesystem::path> requested;
    bool failure = false;

private:
    bool acquired;
};

}

static void CheckModeParsing() {
    Require(ParseInstanceLockMode(nullptr) == InstanceLockMode::Off, "an unset variable must be off");
    Require(ParseInstanceLockMode("") == InstanceLockMode::Off, "an empty value must be off");
    Require(ParseInstanceLockMode("off") == InstanceLockMode::Off, "off must be off");
    Require(ParseInstanceLockMode("on") == InstanceLockMode::On, "on must be on");
}

static void CheckModeRejection() {
    for (const char* value : {"1", "ON", "auto", "true", " on", "on ", "0"}) {
        bool thrown = false;
        try {
            ParseInstanceLockMode(value);
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            thrown = true;
            Require(message.find("ANYPS5_INSTANCE_LOCK") != std::string::npos, std::string("the error for '") + value + "' must name the variable: " + message);
            Require(message.find(std::string("'") + value + "'") != std::string::npos, std::string("the error must quote '") + value + "': " + message);
        }
        Require(thrown, std::string("'") + value + "' must be rejected");
    }
}

static void CheckPath() {
    const std::filesystem::path root = std::filesystem::path("data") / "run";
    Require(InstanceLockPath(root) == root / ".anyps5-instance.lock", "the lock file must sit directly in the root");
}

static void CheckAcquired() {
    FakeBackend backend(true);
    AcquireInstanceLock(backend, "root", "PPSA00000");
    Require(backend.requested.size() == 1, "the backend must be asked once");
    Require(backend.requested[0] == InstanceLockPath("root"), "the backend must be asked for the lock file in the root");
}

static void CheckHeld() {
    FakeBackend backend(false);
    bool thrown = false;
    try {
        AcquireInstanceLock(backend, "root", "PPSA00000");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        thrown = true;
        Require(message.find("PPSA00000") != std::string::npos, "the error must name the title ID: " + message);
        Require(message.find(InstanceLockPath("root").string()) != std::string::npos, "the error must name the lock file: " + message);
        Require(message.find("ANYPS5_INSTANCE_LOCK=off") != std::string::npos, "the error must name the variable that turns the check off: " + message);
    }
    Require(thrown, "a held lock must stop the second instance");
}

static void CheckBackendFailurePropagates() {
    FakeBackend backend(true);
    backend.failure = true;
    bool thrown = false;
    try {
        AcquireInstanceLock(backend, "root", "PPSA00000");
    } catch (const std::runtime_error& error) {
        thrown = std::string(error.what()).find("cannot open") != std::string::npos;
    }
    Require(thrown, "a backend failure must reach the caller unchanged");
}

int main() {
    try {
        CheckModeParsing();
        CheckModeRejection();
        CheckPath();
        CheckAcquired();
        CheckHeld();
        CheckBackendFailurePropagates();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

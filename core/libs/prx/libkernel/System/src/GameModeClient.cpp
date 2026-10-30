#include "prx/libkernel/System/include/GameModeClient.hpp"
#include <dlfcn.h>
#include <exception>
#include <stdexcept>
#include <utility>

namespace HostPerformance {

GameModeClient::GameModeClient(std::vector<std::string> libraries) : libraries(std::move(libraries)) {}

GameModeClient::~GameModeClient() {
    try {
        End();
    } catch (const std::exception&) {
    }
    if (handle != nullptr) dlclose(handle);
}

std::vector<std::string> GameModeClient::DefaultLibraries() {
    return {"libgamemode.so.0", "libgamemode.so"};
}

const char* GameModeClient::Name() const {
    return "gamemode";
}

BeginResult GameModeClient::Begin() {
    if (requested) return {Outcome::Started, {}};
    if (handle == nullptr) {
        std::string failures;
        for (const auto& library : libraries) {
            handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (handle != nullptr) break;
            const char* reason = dlerror();
            if (!failures.empty()) failures += "; ";
            failures += reason != nullptr ? reason : library + ": unknown dlopen failure";
        }
        if (handle == nullptr) return {Outcome::Unavailable, failures.empty() ? "no library to load" : failures};
        requestStart = reinterpret_cast<Request>(dlsym(handle, "real_gamemode_request_start"));
        requestEnd = reinterpret_cast<Request>(dlsym(handle, "real_gamemode_request_end"));
        errorString = reinterpret_cast<ErrorString>(dlsym(handle, "real_gamemode_error_string"));
    }
    if (requestStart == nullptr) return {Outcome::Unavailable, "the library has no real_gamemode_request_start"};
    if (requestEnd == nullptr) return {Outcome::Unavailable, "the library has no real_gamemode_request_end"};
    if (requestStart() != 0) {
        const char* reason = errorString != nullptr ? errorString() : nullptr;
        return {Outcome::Refused, reason != nullptr && *reason != '\0' ? reason : "no reason given"};
    }
    requested = true;
    return {Outcome::Started, {}};
}

void GameModeClient::End() {
    if (!requested) return;
    requested = false;
    if (requestEnd() != 0) {
        const char* reason = errorString != nullptr ? errorString() : nullptr;
        throw std::runtime_error(reason != nullptr && *reason != '\0' ? reason : "no reason given");
    }
}

}

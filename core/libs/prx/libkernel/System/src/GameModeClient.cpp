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
        auto appendFailure = [&](const std::string& message) {
            if (!failures.empty()) failures += "; ";
            failures += message;
        };
        for (const auto& library : libraries) {
            void* candidate = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (candidate == nullptr) {
                const char* reason = dlerror();
                appendFailure(reason != nullptr ? reason : library + ": unknown dlopen failure");
                continue;
            }
            const auto candidateStart = reinterpret_cast<Request>(dlsym(candidate, "real_gamemode_request_start"));
            const auto candidateEnd = reinterpret_cast<Request>(dlsym(candidate, "real_gamemode_request_end"));
            if (candidateStart == nullptr || candidateEnd == nullptr) {
                const char* missingSymbol = candidateStart == nullptr
                    ? "the library has no real_gamemode_request_start"
                    : "the library has no real_gamemode_request_end";
                appendFailure(libraries.size() > 1 ? library + ": " + missingSymbol : missingSymbol);
                dlclose(candidate);
                continue;
            }
            handle = candidate;
            requestStart = candidateStart;
            requestEnd = candidateEnd;
            errorString = reinterpret_cast<ErrorString>(dlsym(candidate, "real_gamemode_error_string"));
            break;
        }
        if (handle == nullptr) return {Outcome::Unavailable, failures.empty() ? "no library to load" : failures};
    }
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

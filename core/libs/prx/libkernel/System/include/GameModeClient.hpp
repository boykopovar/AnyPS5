#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_GAMEMODECLIENT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_GAMEMODECLIENT_HPP

#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <string>
#include <vector>

namespace HostPerformance {

class GameModeClient final : public IBackend {
public:
    explicit GameModeClient(std::vector<std::string> libraries = DefaultLibraries());
    ~GameModeClient() override;
    GameModeClient(const GameModeClient&) = delete;
    GameModeClient& operator=(const GameModeClient&) = delete;

    static std::vector<std::string> DefaultLibraries();

    const char* Name() const override;
    BeginResult Begin() override;
    void End() override;

private:
    using Request = int (*)();
    using ErrorString = const char* (*)();

    std::vector<std::string> libraries;
    void* handle = nullptr;
    Request requestStart = nullptr;
    Request requestEnd = nullptr;
    ErrorString errorString = nullptr;
    bool requested = false;
};

}

#endif

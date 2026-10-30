#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTPERFORMANCE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_HOSTPERFORMANCE_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace HostPerformance {

enum class Mode { Off, On, Auto };

Mode ParseMode(const char* text);

enum class Outcome { Started, Unavailable, Refused };

struct BeginResult {
    Outcome Result = Outcome::Unavailable;
    std::string Detail;
};

class IBackend {
public:
    virtual ~IBackend() = default;
    virtual const char* Name() const = 0;
    virtual BeginResult Begin() = 0;
    virtual void End() = 0;
};

enum class State { Idle, Off, Active, Unavailable, Refused, Ended };

class Session {
public:
    using Log = std::function<void(const std::string&)>;

    Session(Mode mode, IBackend& backend, Log log);
    State Start();
    void Stop();
    State Current() const;

private:
    Mode mode;
    IBackend& backend;
    Log log;
    mutable std::mutex mutex;
    State state = State::Idle;
};

std::unique_ptr<IBackend> CreateHostBackend();
Session* StartProcessSession(Mode mode, std::unique_ptr<IBackend> backend);
void StartFromEnvironment();

}

#endif

#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace AgcDriver {

namespace {

ThreadPriorityMode RequestedMode() {
    static const ThreadPriorityMode mode = ParseThreadPriorityMode(std::getenv("APS5_THREAD_PRIORITY"));
    return mode;
}

int EnvInt(const char* name, int fallback) {
    const char* text = std::getenv(name);
    return text != nullptr && *text != '\0' ? std::atoi(text) : fallback;
}

void LogOnce(const char* role, const std::string& text) {
    static std::mutex mutex;
    static std::set<std::string> seen;
    std::lock_guard lock(mutex);
    if (!seen.insert(std::string(role) + "|" + text).second) return;
    std::fprintf(stderr, "[thread-priority] %s: %s\n", role, text.c_str());
}

#ifndef _WIN32

struct DBusConnection;
struct DBusMessage;
struct DBusError {
    const char* name;
    const char* message;
    unsigned int dummy1 : 1, dummy2 : 1, dummy3 : 1, dummy4 : 1, dummy5 : 1;
    void* padding1;
};
constexpr int DbusBusSystem = 1;
constexpr int DbusTypeInvalid = 0;
constexpr int DbusTypeInt32 = static_cast<int>('i');
constexpr int DbusTypeUint32 = static_cast<int>('u');
constexpr int DbusTypeUint64 = static_cast<int>('t');
constexpr int DbusTypeInt64 = static_cast<int>('x');
constexpr int DbusTypeString = static_cast<int>('s');
constexpr int DbusTypeVariant = static_cast<int>('v');

struct DBusMessageIter {
    void* dummy1;
    void* dummy2;
    std::uint32_t dummy3;
    int dummy4, dummy5, dummy6, dummy7, dummy8, dummy9, dummy10, dummy11;
    int pad1;
    void* pad2;
    void* pad3;
};

struct Dbus {
    void* handle = nullptr;
    DBusConnection* connection = nullptr;
    int (*threadsInitDefault)() = nullptr;
    void (*errorInit)(DBusError*) = nullptr;
    void (*errorFree)(DBusError*) = nullptr;
    int (*errorIsSet)(const DBusError*) = nullptr;
    DBusConnection* (*busGet)(int, DBusError*) = nullptr;
    DBusMessage* (*messageNewMethodCall)(const char*, const char*, const char*, const char*) = nullptr;
    int (*messageAppendArgs)(DBusMessage*, int, ...) = nullptr;
    DBusMessage* (*sendWithReplyAndBlock)(DBusConnection*, DBusMessage*, int, DBusError*) = nullptr;
    void (*messageUnref)(DBusMessage*) = nullptr;
    int (*iterInit)(DBusMessage*, DBusMessageIter*) = nullptr;
    int (*iterGetArgType)(DBusMessageIter*) = nullptr;
    void (*iterRecurse)(DBusMessageIter*, DBusMessageIter*) = nullptr;
    void (*iterGetBasic)(DBusMessageIter*, void*) = nullptr;
    std::string failure;
};

Dbus& LoadDbus() {
    static Dbus dbus = [] {
        Dbus loaded;
        loaded.handle = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
        if (loaded.handle == nullptr) {
            loaded.failure = "libdbus-1.so.3 is not available";
            return loaded;
        }
        const auto resolve = [&](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(dlsym(loaded.handle, name));
            if (function == nullptr && loaded.failure.empty()) loaded.failure = std::string("libdbus lacks ") + name;
        };
        resolve(loaded.threadsInitDefault, "dbus_threads_init_default");
        resolve(loaded.errorInit, "dbus_error_init");
        resolve(loaded.errorFree, "dbus_error_free");
        resolve(loaded.errorIsSet, "dbus_error_is_set");
        resolve(loaded.busGet, "dbus_bus_get");
        resolve(loaded.messageNewMethodCall, "dbus_message_new_method_call");
        resolve(loaded.messageAppendArgs, "dbus_message_append_args");
        resolve(loaded.sendWithReplyAndBlock, "dbus_connection_send_with_reply_and_block");
        resolve(loaded.messageUnref, "dbus_message_unref");
        resolve(loaded.iterInit, "dbus_message_iter_init");
        resolve(loaded.iterGetArgType, "dbus_message_iter_get_arg_type");
        resolve(loaded.iterRecurse, "dbus_message_iter_recurse");
        resolve(loaded.iterGetBasic, "dbus_message_iter_get_basic");
        if (!loaded.failure.empty()) return loaded;
        loaded.threadsInitDefault();
        DBusError error{};
        loaded.errorInit(&error);
        loaded.connection = loaded.busGet(DbusBusSystem, &error);
        if (loaded.connection == nullptr) {
            loaded.failure = std::string("system bus: ") + (loaded.errorIsSet(&error) && error.message != nullptr ? error.message : "unavailable");
            loaded.errorFree(&error);
        }
        return loaded;
    }();
    return dbus;
}

DBusMessage* RtkitSend(DBusMessage* message, const char* method, std::string& failure) {
    auto& dbus = LoadDbus();
    DBusError error{};
    dbus.errorInit(&error);
    auto* reply = dbus.sendWithReplyAndBlock(dbus.connection, message, 5000, &error);
    dbus.messageUnref(message);
    if (reply == nullptr) {
        failure = std::string(method) + ": " + (dbus.errorIsSet(&error) && error.message != nullptr ? error.message : "no reply");
        dbus.errorFree(&error);
    }
    return reply;
}

bool RtkitCall(const char* method, std::uint64_t tid, int type, const void* value, std::string& failure) {
    auto& dbus = LoadDbus();
    if (dbus.connection == nullptr) {
        failure = dbus.failure;
        return false;
    }
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    auto* message = dbus.messageNewMethodCall("org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1", "org.freedesktop.RealtimeKit1", method);
    if (message == nullptr) {
        failure = "dbus message allocation failed";
        return false;
    }
    const std::uint64_t pid = static_cast<std::uint64_t>(getpid());
    bool ok = false;
    if (type == DbusTypeInt32) ok = dbus.messageAppendArgs(message, DbusTypeUint64, &pid, DbusTypeUint64, &tid, DbusTypeInt32, value, DbusTypeInvalid) != 0;
    else ok = dbus.messageAppendArgs(message, DbusTypeUint64, &pid, DbusTypeUint64, &tid, DbusTypeUint32, value, DbusTypeInvalid) != 0;
    if (!ok) {
        dbus.messageUnref(message);
        failure = "dbus argument append failed";
        return false;
    }
    auto* reply = RtkitSend(message, method, failure);
    if (reply == nullptr) return false;
    dbus.messageUnref(reply);
    return true;
}

bool RtkitProperty(const char* name, std::int64_t& value, std::string& failure) {
    auto& dbus = LoadDbus();
    if (dbus.connection == nullptr) {
        failure = dbus.failure;
        return false;
    }
    auto* message = dbus.messageNewMethodCall("org.freedesktop.RealtimeKit1", "/org/freedesktop/RealtimeKit1", "org.freedesktop.DBus.Properties", "Get");
    if (message == nullptr) {
        failure = "dbus message allocation failed";
        return false;
    }
    const char* interfaceName = "org.freedesktop.RealtimeKit1";
    if (dbus.messageAppendArgs(message, DbusTypeString, &interfaceName, DbusTypeString, &name, DbusTypeInvalid) == 0) {
        dbus.messageUnref(message);
        failure = "dbus argument append failed";
        return false;
    }
    auto* reply = RtkitSend(message, name, failure);
    if (reply == nullptr) return false;
    DBusMessageIter iter{};
    DBusMessageIter variant{};
    bool ok = false;
    if (dbus.iterInit(reply, &iter) != 0 && dbus.iterGetArgType(&iter) == DbusTypeVariant) {
        dbus.iterRecurse(&iter, &variant);
        const int type = dbus.iterGetArgType(&variant);
        if (type == DbusTypeInt32) {
            std::int32_t number = 0;
            dbus.iterGetBasic(&variant, &number);
            value = number;
            ok = true;
        } else if (type == DbusTypeInt64) {
            std::int64_t number = 0;
            dbus.iterGetBasic(&variant, &number);
            value = number;
            ok = true;
        }
    }
    dbus.messageUnref(reply);
    if (!ok) failure = std::string(name) + ": unexpected reply type";
    return ok;
}

bool PrepareRealtime(std::uint32_t priority, const char* role, std::string& failure) {
    std::int64_t maxPriority = 0;
    std::int64_t maxRttime = 0;
    std::string rtkitFailure;
    std::optional<RtkitLimits> rtkit;
    if (RtkitProperty("MaxRealtimePriority", maxPriority, rtkitFailure) && RtkitProperty("RTTimeUSecMax", maxRttime, rtkitFailure)) rtkit = RtkitLimits{maxPriority, maxRttime};
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    rlimit limit{};
    if (getrlimit(RLIMIT_RTTIME, &limit) != 0) {
        failure = "getrlimit(RLIMIT_RTTIME) failed";
        return false;
    }
    const auto requested = static_cast<std::uint64_t>(std::max(1, EnvInt("APS5_THREAD_RTTIME_US", 200000)));
    const auto plan = PlanRealtime(priority, rtkit, {limit.rlim_cur, limit.rlim_max}, requested);
    if (!plan.refusal.empty()) {
        failure = rtkit.has_value() ? plan.refusal : rtkitFailure;
        return false;
    }
    sched_param param{};
    if (sched_getparam(0, &param) != 0 || sched_setscheduler(0, sched_getscheduler(0) | SCHED_RESET_ON_FORK, &param) != 0) {
        failure = "SCHED_RESET_ON_FORK could not be set";
        return false;
    }
    if (!plan.limit.has_value()) return true;
    const rlimit wanted{plan.limit->soft, plan.limit->hard};
    if (setrlimit(RLIMIT_RTTIME, &wanted) != 0) {
        failure = "setrlimit(RLIMIT_RTTIME) failed";
        return false;
    }
    if (wanted.rlim_max != limit.rlim_max) {
        const auto text = [](rlim_t value) { return value == RLIM_INFINITY ? std::string("unlimited") : std::to_string(value); };
        LogOnce(role, "RLIMIT_RTTIME hard limit lowered from " + text(limit.rlim_max) + " to " + text(wanted.rlim_max) + " us for the process");
    }
    return true;
}

void Raise(const char* role) {
    const auto tid = static_cast<std::uint64_t>(syscall(SYS_gettid));
    std::string failure;
    if (RequestedMode() == ThreadPriorityMode::Realtime) {
        const std::uint32_t priority = static_cast<std::uint32_t>(std::max(1, std::min(99, EnvInt("APS5_THREAD_RT", 10))));
        if (!PrepareRealtime(priority, role, failure) || !RtkitCall("MakeThreadRealtimeWithPID", tid, DbusTypeUint32, &priority, failure)) {
            throw std::runtime_error(std::string("APS5_THREAD_PRIORITY=rt: ") + role + " realtime refused: " + failure);
        }
        const int policy = sched_getscheduler(0) & ~SCHED_RESET_ON_FORK;
        LogOnce(role, std::string(policy == SCHED_FIFO ? "SCHED_FIFO " : "SCHED_RR ") + std::to_string(priority) + " via rtkit");
        return;
    }
    const std::int32_t nice = static_cast<std::int32_t>(std::max(-20, std::min(19, EnvInt("APS5_THREAD_NICE", -10))));
    if (!RtkitCall("MakeThreadHighPriorityWithPID", tid, DbusTypeInt32, &nice, failure)) {
        throw std::runtime_error(std::string("APS5_THREAD_PRIORITY=high: ") + role + " nice " + std::to_string(nice) + " refused: " + failure);
    }
    LogOnce(role, "nice " + std::to_string(nice) + " via rtkit");
}

#else

void Raise(const char* role) {
    const int priority = RequestedMode() == ThreadPriorityMode::Realtime ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_HIGHEST;
    if (!SetThreadPriority(GetCurrentThread(), priority)) {
        throw std::runtime_error(std::string("APS5_THREAD_PRIORITY: ") + role + " SetThreadPriority failed (error " + std::to_string(GetLastError()) + ")");
    }
    LogOnce(role, priority == THREAD_PRIORITY_TIME_CRITICAL ? "time-critical" : "highest");
}

#endif

}

ThreadPriorityMode ParseThreadPriorityMode(const char* text) {
    if (text == nullptr || *text == '\0' || std::strcmp(text, "off") == 0 || std::strcmp(text, "0") == 0) return ThreadPriorityMode::Off;
    if (std::strcmp(text, "rt") == 0 || std::strcmp(text, "realtime") == 0) return ThreadPriorityMode::Realtime;
    return ThreadPriorityMode::High;
}

RealtimePlan PlanRealtime(std::uint32_t priority, const std::optional<RtkitLimits>& rtkit, RttimeLimit current, std::uint64_t requestedUs) {
    if (!rtkit.has_value()) return {"rtkit's limits are unknown", std::nullopt};
    if (static_cast<std::int64_t>(priority) > rtkit->maxPriority) return {"priority " + std::to_string(priority) + " is above rtkit's MaxRealtimePriority " + std::to_string(rtkit->maxPriority), std::nullopt};
    if (rtkit->maxRttimeUs <= 0) return {"rtkit reports RTTimeUSecMax " + std::to_string(rtkit->maxRttimeUs), std::nullopt};
    const auto ceiling = std::min(requestedUs, static_cast<std::uint64_t>(rtkit->maxRttimeUs));
    auto wanted = current;
    if (wanted.hard > ceiling) wanted.hard = ceiling;
    if (wanted.soft > wanted.hard / 2) wanted.soft = wanted.hard / 2;
    if (wanted == current) return {};
    return {{}, wanted};
}

void RaiseWorkerThreadPriority(const char* role) {
    if (RequestedMode() == ThreadPriorityMode::Off) return;
    Raise(role);
}

}

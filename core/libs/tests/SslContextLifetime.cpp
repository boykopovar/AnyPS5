#include "prx/libSceSsl/Export.cpp"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdio>
#include <latch>
#include <thread>

extern "C" bool KernelLookupSocketNative_nid_no_patch(int, std::shared_ptr<void>*, std::uintptr_t*) {
    std::fputs("unexpected native socket lookup in SSL lifetime test\n", stderr);
    std::abort();
}

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int CreateContext() {
    const int id = sceSslInit_nid_postfix(1024 * 1024);
    Check(id > 0, "context creation failed");
    return id;
}

int CreateConnection(int context) {
    const int id = sceSslCreateConnection(context, -1, "localhost");
    Check(id > 0, "connection creation failed");
    return id;
}

void DeleteBeforeTerm() {
    for (int cycle = 0; cycle < 3; ++cycle) {
        const int context = CreateContext();
        const int connection = CreateConnection(context);
        Check(sceSslDeleteConnection(connection) == 0, "delete before term failed");
        Check(sceSslTerm_nid_postfix(context) == 0, "term after delete failed");
        Check(State().connections.empty() && State().contexts.empty(), "delete-first cycle retained records");
    }
    std::puts("SSL delete-first control: 3 cycles, no retained records");
}

void TermBeforeDelete() {
    bool clean = true;
    for (int cycle = 0; cycle < 3; ++cycle) {
        const auto before = State().connections.size();
        const int context = CreateContext();
        const int connection = CreateConnection(context);
        const auto created = State().connections.size();
        Check(sceSslTerm_nid_postfix(context) == 0, "term before delete failed");
        const auto terminated = State().connections.size();
        Check(sceSslDeleteConnection(connection) == ErrorInvalidId, "post-term delete result changed");
        const auto deleted = State().connections.size();
        Check(State().contexts.empty(), "context survived termination");
        clean = clean && before == 0 && created == 1 && terminated == 0 && deleted == 0;
        std::printf("SSL term-first cycle %d: connections %zu -> %zu -> %zu -> %zu\n", cycle + 1, before, created, terminated, deleted);
    }
    Check(clean, "termination retained orphan connection records");
}

void SeparateContexts() {
    const int first = CreateContext();
    const int second = CreateContext();
    const int firstConnection = CreateConnection(first);
    const int secondConnection = CreateConnection(second);
    const int extraConnection = CreateConnection(first);
    std::weak_ptr<Context> lifetime = State().contexts.at(first);
    Check(sceSslTerm_nid_postfix(first) == 0, "selective term failed");
    Check(lifetime.expired(), "idle terminated context survived");
    Check(State().connections.size() == 1 && State().connections.contains(secondConnection), "term did not isolate connection ownership");
    Check(!State().connections.contains(firstConnection) && !State().connections.contains(extraConnection), "term retained a child connection");
    Check(sceSslTerm_nid_postfix(first) == ErrorInvalidId, "repeated term result changed");
    Check(sceSslCreateConnection(first, -1, "localhost") == ErrorInvalidId, "create accepted a terminated context");
    Check(sceSslDeleteConnection(firstConnection) == ErrorInvalidId, "delete accepted a terminated connection");
    Check(sceSslSetVerifyCallback(firstConnection, nullptr, nullptr) == ErrorInvalidId, "callback accepted a terminated connection");
    Check(sceSslConnect(firstConnection) == ErrorInvalidId && sceSslClose(firstConnection) == ErrorInvalidId, "I/O accepted a terminated connection");
    Check(sceSslSetVerifyCallback(secondConnection, nullptr, nullptr) == 0, "term invalidated another context's connection");
    Check(sceSslDeleteConnection(secondConnection) == 0 && sceSslTerm_nid_postfix(second) == 0, "unrelated context cleanup failed");
    Check(State().connections.empty() && State().contexts.empty(), "selective term retained records");
}

void ConcurrentLifecycle() {
    for (int cycle = 0; cycle < 32; ++cycle) {
        const int context = CreateContext();
        std::array<int, 8> connections{};
        for (auto& id : connections) id = CreateConnection(context);
        std::barrier start(3);
        std::atomic<bool> valid{true};
        std::thread creator([&] {
            start.arrive_and_wait();
            for (int attempt = 0; attempt < 16; ++attempt) {
                const int id = sceSslCreateConnection(context, -1, "localhost");
                if (id == ErrorInvalidId) continue;
                if (id <= 0) { valid = false; continue; }
                const int result = sceSslDeleteConnection(id);
                if (result != 0 && result != ErrorInvalidId) valid = false;
            }
        });
        std::thread deleter([&] {
            start.arrive_and_wait();
            for (const auto id : connections) {
                const int result = sceSslDeleteConnection(id);
                if (result != 0 && result != ErrorInvalidId) valid = false;
            }
        });
        start.arrive_and_wait();
        const int result = sceSslTerm_nid_postfix(context);
        creator.join();
        deleter.join();
        Check(result == 0 && valid, "concurrent lifecycle returned an unexpected error");
        Check(State().connections.empty() && State().contexts.empty(), "concurrent termination retained records");
    }
    std::puts("SSL concurrent create/delete/term: 32 cycles, no retained records");
}

struct CallbackState {
    std::latch entered{1};
    std::latch resume{1};
    int context = 0;
    bool valid = false;
};

int APS5_VABI PendingCallback(int context, unsigned int, void**, int count, void* argument) {
    auto& state = *static_cast<CallbackState*>(argument);
    state.valid = context == state.context && count == 0;
    state.entered.count_down();
    state.resume.wait();
    return 0;
}

void InFlightSession() {
    const int context = CreateContext();
    const int connection = CreateConnection(context);
    auto owner = State().contexts.at(context);
    std::weak_ptr<Context> contextLifetime = owner;
    auto session = std::make_shared<Session>(context, owner, std::shared_ptr<void>{}, 0);
    {
        PoolScope scope(owner->pool);
        session->Configure("localhost", "", InitialVerifyOptions);
    }
    Check(owner->pool.current > 0, "session did not allocate backend state");
    session->verified = true;
    session->verifyResult = ErrorCallbackCheck;
    State().connections.at(connection).session = session;
    std::weak_ptr<Session> sessionLifetime = session;
    CallbackState callback;
    callback.context = context;
    Check(sceSslSetVerifyCallback(connection, PendingCallback, &callback) == 0, "callback setup failed");
    owner.reset();
    session.reset();
    int operationResult = ErrorInvalidId;
    std::thread operation([&] { operationResult = sceSslConnect(connection); });
    callback.entered.wait();
    const int result = sceSslTerm_nid_postfix(context);
    const bool removed = State().connections.empty() && State().contexts.empty();
    const bool retained = !sessionLifetime.expired() && !contextLifetime.expired();
    callback.resume.count_down();
    operation.join();
    Check(result == 0 && removed, "term did not remove the in-flight connection");
    Check(retained, "term destroyed an in-flight session or its pool");
    Check(callback.valid && operationResult == 0, "in-flight verification callback did not finish");
    Check(sessionLifetime.expired() && contextLifetime.expired(), "finished operation retained its session or pool");
    Check(sceSslConnect(connection) == ErrorInvalidId && sceSslDeleteConnection(connection) == ErrorInvalidId, "post-term connection became usable");
    std::puts("SSL in-flight callback completed; session and pool released after completion");
}

}

int main() {
    try {
        DeleteBeforeTerm();
        TermBeforeDelete();
        SeparateContexts();
        ConcurrentLifecycle();
        InFlightSession();
        std::puts("SSL context lifetime passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL context lifetime failed: %s\n", error.what());
        return 1;
    }
}

#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct Context {
    PthreadKey key = 0;
    int stored = 0;
    int exitValue = 0;
    bool exitWithValue = true;
    std::atomic<bool> afterExit{false};
    std::atomic<int> sequence{0};
    std::atomic<int> cleanupOrder{0};
    std::atomic<int> destructorOrder{0};
    std::atomic<int> cleanupCalls{0};
    std::atomic<int> destructorCalls{0};
    std::atomic<void*> destructorValue{nullptr};
};

Context* g_context = nullptr;

void APS5_VABI Cleanup(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    context.cleanupOrder = ++context.sequence;
    ++context.cleanupCalls;
}

void APS5_VABI Destroy(void* value) {
    g_context->destructorValue = value;
    g_context->destructorOrder = ++g_context->sequence;
    ++g_context->destructorCalls;
}

void* APS5_VABI Worker(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    Require(pthread_setspecific_nid_postfix(context.key, &context.stored) == 0, "worker setspecific returns 0");
    pthread_cleanup_push_nid_postfix(Cleanup, &context);
    pthread_exit_nid_postfix(context.exitWithValue ? &context.exitValue : nullptr);
    context.afterExit = true;
    return argument;
}

void Run(Context& context) {
    g_context = &context;
    Require(pthread_key_create_nid_postfix(&context.key, reinterpret_cast<pthread_key_destructor_func_t>(Destroy)) == 0, "key create returns 0");
    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, Worker, &context) == 0, "create returns 0");
    void* result = &context;
    Require(pthread_join_nid_postfix(thread, &result) == 0, "join returns 0");
    Require(!context.afterExit, "code after pthread_exit never runs");
    Require(context.destructorCalls == 1, "the key destructor runs once on pthread_exit");
    Require(context.cleanupCalls == 1, "the cleanup handler runs once on pthread_exit");
    Require(context.cleanupOrder < context.destructorOrder, "the cleanup handler runs before the key destructor");
    Require(context.destructorValue == &context.stored, "the key destructor receives the stored value");
    Require(result == (context.exitWithValue ? static_cast<void*>(&context.exitValue) : nullptr), "join returns the value passed to pthread_exit");
    Require(pthread_key_delete_nid_postfix(context.key) == 0, "key delete returns 0");
}

}

int main() {
    Context withValue;
    Run(withValue);
    Context withNull;
    withNull.exitWithValue = false;
    Run(withNull);
    return 0;
}

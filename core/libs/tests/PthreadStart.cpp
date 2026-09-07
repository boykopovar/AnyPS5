#include "prx/libkernel/Pthread/Pthread.hpp"
#include <future>
#include <iostream>
#include <stdexcept>

extern "C" int scePthreadCreate(Pthread* thread, const PthreadAttr* attr, void* (*entry)(void*), void* arg, const char* name);
extern "C" int scePthreadJoin(Pthread thread, void** retval);
extern "C" int scePthreadAttrInit(PthreadAttr* attr);
extern "C" int scePthreadAttrDestroy(PthreadAttr* attr);
extern "C" int scePthreadAttrGet(Pthread thread, PthreadAttr* attr);

struct ThreadContext {
    Pthread thread = nullptr;
    std::promise<void> checked;
};

static void* CheckThread(void* arg) {
    auto& context = *static_cast<ThreadContext*>(arg);
    if (!context.thread) throw std::runtime_error("Thread handle was not published");
    if (context.thread->_thr.get_id() != std::this_thread::get_id()) throw std::runtime_error("Thread object was not initialized");
    if (!context.thread->_thr.joinable()) throw std::runtime_error("Thread object is not joinable");
    PthreadAttr attr = nullptr;
    if (scePthreadAttrInit(&attr) != 0) throw std::runtime_error("Attribute initialization failed");
    if (scePthreadAttrGet(context.thread, &attr) != 0) throw std::runtime_error("Attribute query failed");
    if (scePthreadAttrDestroy(&attr) != 0) throw std::runtime_error("Attribute destruction failed");
    context.checked.set_value();
    return arg;
}

int main() {
    for (int iteration = 0; iteration < 10000; ++iteration) {
        ThreadContext context;
        if (scePthreadCreate(&context.thread, nullptr, CheckThread, &context, nullptr) != 0) throw std::runtime_error("Thread creation failed");
        context.checked.get_future().get();
        void* result = nullptr;
        if (scePthreadJoin(context.thread, &result) != 0) throw std::runtime_error("Thread join failed");
        if (result != &context) throw std::runtime_error("Thread return value was lost");
    }
    std::cout << "PASS: 10000 thread starts, initialized handles, attribute queries and joins\n";
}

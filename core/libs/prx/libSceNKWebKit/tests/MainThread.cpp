#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    Require(!_ZN3WTF12isMainThreadEv());
    Require(!_ZN3WTF13isMainRunLoopEv());
    Require(!_ZN3WTF22isMainThreadOrGCThreadEv());

    _ZN3WTF20initializeMainThreadEv();

    Require(_ZN3WTF12isMainThreadEv());
    Require(_ZN3WTF13isMainRunLoopEv());
    Require(_ZN3WTF22isMainThreadOrGCThreadEv());

    std::atomic<bool> workerIsMainThread{true};
    std::atomic<bool> workerIsMainRunLoop{true};
    std::atomic<bool> workerIsMainOrGC{true};

    std::thread worker([&] {
        workerIsMainThread.store(_ZN3WTF12isMainThreadEv());
        workerIsMainRunLoop.store(_ZN3WTF13isMainRunLoopEv());
        workerIsMainOrGC.store(_ZN3WTF22isMainThreadOrGCThreadEv());
        _ZN3WTF20initializeMainThreadEv();
    });
    worker.join();

    Require(!workerIsMainThread.load());
    Require(!workerIsMainRunLoop.load());
    Require(!workerIsMainOrGC.load());

    Require(_ZN3WTF12isMainThreadEv());
    Require(_ZN3WTF13isMainRunLoopEv());
    Require(_ZN3WTF22isMainThreadOrGCThreadEv());

    std::printf("All libSceNKWebKit MainThread tests passed!\n");
    return 0;
}

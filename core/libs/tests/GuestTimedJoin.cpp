#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static_assert(sizeof(KernelTimespec)==16 && alignof(KernelTimespec)==8 && offsetof(KernelTimespec,tv_nsec)==8);

using TimedJoin = decltype(&pthread_timedjoin_np_nid_postfix);
static TimedJoin timedJoin;

void Require(bool value,const char* message) {
    if (!value) { std::fprintf(stderr,"timed join: %s\n",message); std::abort(); }
}

KernelTimespec After(int milliseconds) {
    KernelTimespec result{};
    Require(clock_gettime_nid_postfix(0,&result)==0,"read realtime");
    result.tv_nsec+=static_cast<std::int64_t>(milliseconds)*1000000;
    result.tv_sec+=result.tv_nsec/1000000000;
    result.tv_nsec%=1000000000;
    return result;
}

struct Gate { std::atomic<bool> release{false}, entered{false}, leaving{false}; };
void* APS5_VABI Worker(void* argument) {
    auto& gate=*static_cast<Gate*>(argument);
    gate.entered.store(true,std::memory_order_release);
    while (!gate.release.load(std::memory_order_acquire)) std::this_thread::yield();
    gate.leaving.store(true,std::memory_order_release);
    return reinterpret_cast<void*>(std::uintptr_t{0x12345678});
}

void Wait(const std::atomic<bool>& flag) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while (!flag.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now()<deadline,"worker handshake timed out");
        std::this_thread::yield();
    }
}

Pthread Start(Gate& gate) {
    Pthread thread=nullptr;
    Require(pthread_create_nid_postfix(&thread,nullptr,Worker,&gate)==0,"create worker");
    Wait(gate.entered);
    return thread;
}

struct JoinTask { Pthread target; std::atomic<bool> entered{false}; int result=-1; void* value=reinterpret_cast<void*>(std::uintptr_t{0x55}); };
void* APS5_VABI Joiner(void* argument) {
    auto& task=*static_cast<JoinTask*>(argument);
    const auto deadline=After(3000);
    task.entered.store(true,std::memory_order_release);
    task.result=timedJoin(task.target,&task.value,&deadline);
    return nullptr;
}

void AwaitClaim(Pthread target) {
    const KernelTimespec past{0,0};
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    for (;;) {
        const int result=timedJoin(target,nullptr,&past);
        if (result==45) return;
        Require(result==60,"claim probe returns timeout or ENOTSUP");
        Require(std::chrono::steady_clock::now()<until,"joiner did not claim target");
        std::this_thread::yield();
    }
}

void Basic() {
    const KernelTimespec valid=After(1000);
    for (const KernelTimespec invalid: {KernelTimespec{-1,0},KernelTimespec{0,-1},KernelTimespec{0,1000000000}}) {
        Require(timedJoin(nullptr,nullptr,&invalid)==22,"invalid deadline precedes null thread");
        Require(timedJoin(pthread_self_nid_postfix(),nullptr,&invalid)==22,"invalid deadline precedes self join");
    }
    Require(timedJoin(nullptr,nullptr,nullptr)==22,"null deadline");
    Require(timedJoin(nullptr,nullptr,&valid)==22,"null thread");
    Require(timedJoin(pthread_self_nid_postfix(),nullptr,&valid)==11,"self join");
    Gate gate;
    Pthread thread=Start(gate);
    struct { std::uint64_t before=0xa5a5a5a5; void* value=reinterpret_cast<void*>(std::uintptr_t{0x55}); std::uint64_t after=0x5a5a5a5a; } result;
    const KernelTimespec past{0,0};
    *__error_nid_postfix()=123;
    Require(timedJoin(thread,&result.value,&past)==60,"past deadline expires");
    Require(result.value==reinterpret_cast<void*>(std::uintptr_t{0x55}),"timeout preserves return slot");
    Require(*__error_nid_postfix()==123,"POSIX error preserves errno");
    const auto deadline=After(50);
    const auto originalDeadline=deadline;
    const auto started=std::chrono::steady_clock::now();
    Require(timedJoin(thread,&result.value,&deadline)==60,"future deadline expires");
    Require(std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(25),"wait did not expire immediately");
    Require(deadline.tv_sec==originalDeadline.tv_sec && deadline.tv_nsec==originalDeadline.tv_nsec,"deadline input is unchanged");
    gate.release.store(true,std::memory_order_release);
    const KernelTimespec distant{std::numeric_limits<std::int64_t>::max(),999999999};
    Require(timedJoin(thread,&result.value,&distant)==0,"timeout leaves target joinable; distant deadline does not overflow");
    Require(result.value==reinterpret_cast<void*>(std::uintptr_t{0x12345678}),"joined return value");
    Require(result.before==0xa5a5a5a5 && result.after==0x5a5a5a5a,"return-slot bounds");
    Require(timedJoin(thread,nullptr,&valid)==3,"already joined target");
    Require(timedJoin(thread,nullptr,nullptr)==22,"deadline validation precedes joined state");
    Gate done;
    Pthread completed=Start(done);
    done.release.store(true,std::memory_order_release);
    Wait(done.leaving);
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    for (;;) {
        const int joined=timedJoin(completed,nullptr,&past);
        if (joined==0) break;
        Require(joined==60 && std::chrono::steady_clock::now()<until,"completed thread joins with an expired deadline");
        std::this_thread::yield();
    }
    Gate detached;
    Pthread detachedThread=Start(detached);
    Require(pthread_detach_nid_postfix(detachedThread)==0,"detach worker");
    Require(timedJoin(detachedThread,nullptr,&valid)==22,"detached target");
    detached.release.store(true,std::memory_order_release);
    Wait(detached.leaving);
}

void Contention(bool cancel) {
    Gate gate;
    Pthread target=Start(gate);
    JoinTask task{target};
    Pthread joiner=nullptr;
    Require(pthread_create_nid_postfix(&joiner,nullptr,Joiner,&task)==0,"create competing joiner");
    Wait(task.entered);
    AwaitClaim(target);
    if (cancel) {
        Require(pthread_cancel_nid_postfix(joiner)==0,"cancel blocked timed join");
        void* canceled=nullptr;
        Require(pthread_join_nid_postfix(joiner,&canceled)==0 && canceled==reinterpret_cast<void*>(std::uintptr_t{1}),"joiner is canceled");
        Require(task.result==-1 && task.value==reinterpret_cast<void*>(std::uintptr_t{0x55}),"canceled join did not return or write result");
        const KernelTimespec past{0,0};
        Require(timedJoin(target,nullptr,&past)==60,"cancellation releases join claim");
        gate.release.store(true,std::memory_order_release);
        const auto deadline=After(1000);
        Require(timedJoin(target,nullptr,&deadline)==0,"cancellation preserves target joinability");
    } else {
        gate.release.store(true,std::memory_order_release);
        Require(pthread_join_nid_postfix(joiner,nullptr)==0,"join successful joiner");
        Require(task.result==0 && task.value==reinterpret_cast<void*>(std::uintptr_t{0x12345678}),"competing join succeeds once");
    }
}

int main(int argc,char** argv) {
    Require(argc==2,"expected kernel path");
#ifdef _WIN32
    const auto library=LoadLibraryA(argv[1]);
    Require(library!=nullptr,"load kernel");
    timedJoin=reinterpret_cast<TimedJoin>(GetProcAddress(library,"pthread_timedjoin_np_nid_postfix"));
#else
    const auto library=dlopen(argv[1],RTLD_NOW);
    Require(library!=nullptr,"load kernel");
    timedJoin=reinterpret_cast<TimedJoin>(dlsym(library,"pthread_timedjoin_np_nid_postfix"));
#endif
    Require(timedJoin!=nullptr,"pthread_timedjoin_np export is missing");
    Basic();
    Contention(false);
    Contention(true);
#ifdef _WIN32
    Require(FreeLibrary(library)!=0,"release kernel lookup handle");
#else
    Require(dlclose(library)==0,"release kernel lookup handle");
#endif
    std::puts("timed join checks passed");
}

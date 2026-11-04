#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

struct Environment { std::array<std::uint32_t,7> x87; std::uint32_t mxcsr; };
static_assert(sizeof(Environment)==32 && alignof(Environment)==4 && offsetof(Environment,mxcsr)==28);
using SetEnvironment=int(APS5_VABI*)(const Environment*);
SetEnvironment setEnvironment;
static_assert(sizeof(Environment)==sizeof(GuestFloatingPointEnvironment) && offsetof(Environment,mxcsr)==offsetof(GuestFloatingPointEnvironment,mxcsr));

void Require(bool condition,const char* message) {
    if(!condition) { std::fprintf(stderr,"fesetenv: %s\n",message); std::abort(); }
}
Environment Read() {
    Environment state;
    asm volatile("fnstenv %0\n\tfldenv %0" : "=m"(state.x87));
    state.mxcsr=_mm_getcsr();
    return state;
}
void Write(const Environment& state) {
    asm volatile("fldenv %0" : : "m"(state.x87) : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    _mm_setcsr(state.mxcsr);
}
bool Same(const Environment& a,const Environment& b) { return a.x87==b.x87 && a.mxcsr==b.mxcsr; }

void StateMatrix() {
    const auto original=Read();
    auto clean=original;
    clean.x87[0]|=0x3f;
    clean.x87[1]&=~0xffu;
    clean.mxcsr=(clean.mxcsr&~0x3fu)|(0x3f<<7);
    for(unsigned precision:{0u,0x200u,0x300u})
        for(unsigned rounding=0;rounding<16;++rounding)
            for(unsigned x87=0;x87<64;++x87)
                for(unsigned sse=0;sse<64;++sse) {
                    auto desired=clean;
                    desired.x87[0]=(desired.x87[0]&~0xf00u)|precision|((rounding&3)<<10);
                    desired.x87[1]=(desired.x87[1]&~0x3fu)|x87;
                    desired.mxcsr=(desired.mxcsr&~0x603fu)|((rounding>>2)<<13)|sse;
                    Write(desired);
                    desired=Read();
                    Write(clean);
                    const auto input=desired;
                    *__error_nid_postfix()=123;
                    const int result=setEnvironment(&desired);
                    const auto after=Read();
                    Write(clean);
                    Require(result==0 && *__error_nid_postfix()==123,"matrix return and errno");
                    Require(Same(after,input),"x87 controls/status/tags/pointers and independent MXCSR restored");
                    Require(std::memcmp(&desired,&input,sizeof(input))==0,"input unchanged");
                }
    for(unsigned x87Mask=0;x87Mask<64;++x87Mask)
        for(unsigned sseMask=0;sseMask<64;++sseMask) {
            auto desired=clean;
            desired.x87[0]=(desired.x87[0]&~0x3fu)|x87Mask;
            desired.mxcsr=(desired.mxcsr&~(0x3fu<<7))|(sseMask<<7);
            Require(setEnvironment(&desired)==0,"independent exception-mask return");
            const auto after=Read();
            Write(clean);
            Require(Same(after,desired),"independent x87/SSE exception masks restored");
        }
    alignas(16) std::array<unsigned char,512> extended{};
    asm volatile("fxsave %0" : "=m"(extended));
    std::uint32_t supported;
    std::memcpy(&supported,extended.data()+28,sizeof(supported));
    if(supported==0) supported=0xffbf;
    for(unsigned mode:{0u,0x40u,0x8000u,0x8040u}) {
        if((mode&~supported)!=0) continue;
        auto desired=clean;
        desired.mxcsr=(desired.mxcsr&~0x8040u)|mode;
        Require(setEnvironment(&desired)==0,"subnormal control return");
        const auto after=Read();
        Write(clean);
        Require(Same(after,desired),"supported DAZ/FTZ restored");
    }
    Write(original);
}

void TagsAndPointers() {
    const auto original=Read();
    asm volatile("fninit\n\tfld1\n\tfldpi" : : : "st","st(1)");
    const auto desired=Read();
    asm volatile("fninit" : : : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    const int result=setEnvironment(&desired);
    const auto after=Read();
    Write(original);
    Require(result==0 && Same(after,desired),"nonempty x87 tag/TOP and instruction pointer restored");
    Environment saved;
    Require(fegetenv_nid_postfix(reinterpret_cast<GuestFloatingPointEnvironment*>(&saved))==0,"save using existing guest fegetenv");
    asm volatile("fninit" : : : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    Require(setEnvironment(&saved)==0,"restore guest fegetenv output");
    const auto restored=Read();
    Write(original);
    Require(Same(saved,restored),"guest fegetenv round trip");
}

void BoundsAndErrors() {
    const auto original=Read();
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto page=static_cast<std::size_t>(info.dwPageSize);
    auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Require(memory!=nullptr,"allocate guard pages");
    DWORD previous;
    Require(VirtualProtect(memory+page,page,PAGE_NOACCESS,&previous)!=0,"protect trailing page");
#else
    const auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* memory=static_cast<unsigned char*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    Require(memory!=MAP_FAILED,"allocate guard pages");
    Require(mprotect(memory+page,page,PROT_NONE)==0,"protect trailing page");
#endif
    auto* end=reinterpret_cast<Environment*>(memory+page-sizeof(Environment));
    std::memcpy(end,&original,sizeof(original));
#ifdef _WIN32
    Require(VirtualProtect(memory,page,PAGE_READONLY,&previous)!=0,"input read-only");
#else
    Require(mprotect(memory,page,PROT_READ)==0,"input read-only");
#endif
    const int result=setEnvironment(end);
    const auto after=Read();
    Require(result==0 && Same(after,original),"exact32-byte read at guard-page boundary");
#ifdef _WIN32
    Require(VirtualFree(memory,0,MEM_RELEASE)!=0,"release guard pages");
#else
    Require(munmap(memory,2*page)==0,"release guard pages");
#endif
    bool rejected=false;
    *__error_nid_postfix()=123;
    try { setEnvironment(nullptr); } catch(const std::invalid_argument&) { rejected=true; }
    Require(rejected && Same(Read(),original) && *__error_nid_postfix()==123,"null rejected without state or errno change");
    alignas(16) std::array<std::uint32_t,128> extended;
    asm volatile("fxsave %0" : "=m"(extended));
    const auto supported=extended[7] ? extended[7] : 0xffbfu;
    for(unsigned bit=0;bit<32;++bit) {
        const auto reserved=1u<<bit;
        if((supported&reserved)!=0) continue;
        auto desired=original;
        desired.x87[0]^=0xc00;
        desired.mxcsr|=reserved;
        rejected=false;
        try { setEnvironment(&desired); } catch(const std::invalid_argument&) { rejected=true; }
        Require(rejected && Same(Read(),original) && *__error_nid_postfix()==123,"host-unsupported MXCSR rejected before x87 changes");
    }
}

void Threads() {
    std::barrier ready(2);
    auto worker=[&](unsigned rounding,unsigned flags) {
        const auto original=Read();
        auto desired=original;
        desired.x87[0]=(desired.x87[0]&~0xc00u)|(rounding<<10)|0x3f;
        desired.x87[1]=(desired.x87[1]&~0xffu)|flags;
        desired.mxcsr=(desired.mxcsr&~0x603fu)|(rounding<<13)|(flags^0x3f)|(0x3f<<7);
        ready.arrive_and_wait();
        *__error_nid_postfix()=static_cast<int>(flags);
        const int result=setEnvironment(&desired);
        const int error=*__error_nid_postfix();
        ready.arrive_and_wait();
        const auto after=Read();
        Write(original);
        Require(result==0 && Same(after,desired),"thread-local environment");
        Require(error==static_cast<int>(flags),"thread-local errno");
    };
    std::thread a(worker,1,4),b(worker,2,8);
    a.join(); b.join();
}

volatile std::sig_atomic_t returned=0;
#ifdef _WIN32
DWORD expectedException;
unsigned expectedFlag;
LONG CALLBACK Trap(EXCEPTION_POINTERS* exception) {
    const auto code=exception->ExceptionRecord->ExceptionCode;
    const auto& state=exception->ContextRecord->FltSave;
    const auto active=state.StatusWord & ~state.ControlWord & 0x3f;
    const bool denormalCode=expectedFlag==2 && code==EXCEPTION_FLT_INVALID_OPERATION;
    if(returned && (code==expectedException || denormalCode) && active==expectedFlag) std::_Exit(0);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
void Trap(int) { std::_Exit(returned ? 0 : 1); }
#endif

void Pending(unsigned bit) {
    Require(bit<6,"pending exception bit");
#ifdef _WIN32
    constexpr DWORD codes[]{EXCEPTION_FLT_INVALID_OPERATION,EXCEPTION_FLT_DENORMAL_OPERAND,EXCEPTION_FLT_DIVIDE_BY_ZERO,EXCEPTION_FLT_OVERFLOW,EXCEPTION_FLT_UNDERFLOW,EXCEPTION_FLT_INEXACT_RESULT};
    expectedException=codes[bit];
    expectedFlag=1u<<bit;
    Require(AddVectoredExceptionHandler(1,Trap)!=nullptr,"install trap handler");
#else
    Require(std::signal(SIGFPE,Trap)!=SIG_ERR,"install trap handler");
#endif
    auto desired=Read();
    desired.x87[0]=(desired.x87[0]|0x3f)&~(1u<<bit);
    desired.x87[1]=(desired.x87[1]&~0xffu)|(1u<<bit);
    desired.mxcsr=(desired.mxcsr&~0x3fu)|(0x3f<<7);
    Require(setEnvironment(&desired)==0,"pending exception restore returns");
    returned=1;
    asm volatile("fwait");
    Require(false,"restored pending exception did not trap on subsequent fwait");
}

int main(int argc,char** argv) {
    Require(argc==2 || argc==3,"expected libc path and optional pending exception bit");
#ifdef _WIN32
    const auto library=LoadLibraryA(argv[1]);
    Require(library!=nullptr,"load libc");
    setEnvironment=reinterpret_cast<SetEnvironment>(GetProcAddress(library,"fesetenv_nid_postfix"));
#else
    const auto library=dlopen(argv[1],RTLD_NOW);
    Require(library!=nullptr,"load libc");
    setEnvironment=reinterpret_cast<SetEnvironment>(dlsym(library,"fesetenv_nid_postfix"));
#endif
    Require(setEnvironment!=nullptr,"fesetenv export is missing");
    if(argc==3) Pending(static_cast<unsigned>(std::atoi(argv[2])));
    StateMatrix(); TagsAndPointers(); BoundsAndErrors(); Threads();
#ifdef _WIN32
    Require(FreeLibrary(library)!=0,"release lookup handle");
#else
    Require(dlclose(library)==0,"release lookup handle");
#endif
    std::puts("fesetenv checks passed:196608 state combinations,4096 mask pairs, bounds/errors/tags/threads");
}

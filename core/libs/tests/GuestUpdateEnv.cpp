#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

struct Environment {
    std::array<std::uint32_t,7> x87;
    std::uint32_t mxcsr;
};
static_assert(sizeof(Environment)==32 && offsetof(Environment,mxcsr)==28);
using UpdateEnvironment=int (APS5_VABI*)(const Environment*);

UpdateEnvironment updateEnvironment;

void Require(bool condition,const char* message) {
    if(!condition) { std::fprintf(stderr,"feupdateenv: %s\n",message); std::exit(1); }
}
Environment Read() {
    Environment result;
    asm volatile("fnstenv %0\n\tfldcw %0" : "=m"(result.x87));
    result.mxcsr=_mm_getcsr();
    return result;
}
void Write(const Environment& value) {
    asm volatile("fldenv %0" : : "m"(value.x87) : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    _mm_setcsr(value.mxcsr);
}
bool Same(const Environment& a,const Environment& b){return a.x87==b.x87 && a.mxcsr==b.mxcsr;}
void Matrix() {
    const auto original=Read();
    auto clean=original;
    clean.x87[0]|=0x3f;
    clean.x87[1]&=~0xffu;
    clean.mxcsr=(clean.mxcsr&~0x3fu)|(0x3f<<7);
    unsigned cases=0;
    for(unsigned x87=0;x87<64;++x87)
        for(unsigned sse=0;sse<64;++sse)
            for(unsigned pattern=0;pattern<4;++pattern)
                for(unsigned rounding=0;rounding<16;++rounding) {
                    auto saved=clean;
                    constexpr unsigned precisions[]{0u,0x200u,0x300u,0u};
                    saved.x87[0]=(saved.x87[0]&~0x300u)|precisions[pattern];
                    saved.x87[0]=(saved.x87[0]&~0xc00u)|((rounding&3)<<10);
                    saved.mxcsr=(saved.mxcsr&~0x6000u)|((rounding>>2)<<13);
                    const unsigned savedX87=pattern==0?0:pattern==1?0x15:pattern==2?0x2a:0x3f;
                    const unsigned savedSse=pattern==0?0x3f:pattern==1?0x2a:pattern==2?0x15:0;
                    saved.x87[1]|=savedX87;
                    saved.mxcsr|=savedSse;
                    Write(saved);
                    saved=Read();
                    auto pending=clean;
                    pending.x87[1]|=x87;
                    pending.mxcsr|=sse;
                    Write(pending);
                    const auto input=saved;
                    *__error_nid_postfix()=123;
                    const int result=updateEnvironment(&saved);
                    const auto after=Read();
                    const int error=*__error_nid_postfix();
                    Write(clean);
                    auto expected=input;
                    expected.x87[1]|=x87|sse;
                    expected.mxcsr|=x87|sse;
                    Require(result==0 && error==123,"return/errno");
                    Require(after.x87==expected.x87 && after.mxcsr==expected.mxcsr,"restore saved controls and raise old union in both units");
                    Require(std::memcmp(&input,&saved,sizeof(input))==0,"input unchanged");
                    ++cases;
                }
    Write(original);
    std::printf("feupdateenv: %u flag/rounding combinations passed\n",cases);
}

void TagsAndPointers() {
    const auto entry=Read();
    auto clean=entry;
    clean.x87[0]|=0x3f; clean.x87[1]&=~0xffu;
    clean.mxcsr=(clean.mxcsr&~0x3fu)|(0x3f<<7);
    Write(clean);
    const auto original=Read();
    asm volatile("fninit\n\tfld1\n\tfldpi" : : : "st","st(1)");
    const auto desired=Read();
    asm volatile("fninit" : : : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    const int result=updateEnvironment(&desired);
    const auto after=Read();
    Write(original);
    Require(result==0 && Same(after,desired),"nonempty x87 tag/TOP and instruction pointer restored");
    GuestFloatingPointEnvironment guestSaved;
    Require(fegetenv_nid_postfix(&guestSaved)==0,"save using existing guest fegetenv");
    asm volatile("fninit" : : : "st","st(1)","st(2)","st(3)","st(4)","st(5)","st(6)","st(7)");
    Environment saved;
    std::memcpy(&saved, &guestSaved, sizeof(saved));
    Require(updateEnvironment(&saved)==0,"restore guest fegetenv output");
    const auto restored=Read();
    Write(original);
    Require(Same(saved,restored),"guest fegetenv round trip");
    Write(entry);
}

void BoundsAndErrors() {
    const auto entry=Read();
    auto clean=entry;
    clean.x87[0]|=0x3f; clean.x87[1]&=~0xffu;
    clean.mxcsr=(clean.mxcsr&~0x3fu)|(0x3f<<7);
    Write(clean);
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
    const int result=updateEnvironment(end);
    const auto after=Read();
    Require(result==0 && Same(after,original),"exact32-byte read at guard-page boundary");
#ifdef _WIN32
    Require(VirtualFree(memory,0,MEM_RELEASE)!=0,"release guard pages");
#else
    Require(munmap(memory,2*page)==0,"release guard pages");
#endif
    bool rejected=false;
    *__error_nid_postfix()=123;
    try { updateEnvironment(nullptr); } catch(const std::invalid_argument&) { rejected=true; }
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
        try { updateEnvironment(&desired); } catch(const std::invalid_argument&) { rejected=true; }
        Require(rejected && Same(Read(),original) && *__error_nid_postfix()==123,"host-unsupported MXCSR rejected before x87 changes");
    }
    Write(entry);
}

void MasksAndThreads() {
    const auto original=Read();
    auto clean=original;
    clean.x87[0]|=0x3f; clean.x87[1]&=~0xffu;
    clean.mxcsr=(clean.mxcsr&~0x3fu)|(0x3f<<7);
    for(unsigned a=0;a<64;++a) for(unsigned b=0;b<64;++b) {
        auto desired=clean;
        desired.x87[0]=(desired.x87[0]&~0x3fu)|a;
        desired.mxcsr=(desired.mxcsr&~(0x3fu<<7))|(b<<7);
        Write(clean);
        Require(updateEnvironment(&desired)==0,"independent exception masks");
        const auto actual=Read(); Write(clean);
        Require(Same(actual,desired),"exception masks preserved");
    }
    alignas(16) std::array<std::uint32_t,128> state;
    asm volatile("fxsave %0" : "=m"(state));
    const auto supported=state[7]?state[7]:0xffbfu;
    for(unsigned mode:{0u,0x40u,0x8000u,0x8040u}) {
        if(mode&~supported) continue;
        auto desired=clean; desired.mxcsr=(desired.mxcsr&~0x8040u)|mode;
        Require(updateEnvironment(&desired)==0,"supported DAZ/FTZ return");
        const auto actual=Read(); Write(clean);
        Require(Same(actual,desired),"supported DAZ/FTZ restored");
    }
    std::barrier ready(2);
    auto worker=[&](unsigned rounding,unsigned flags) {
        const auto entry=Read();
        auto desired=clean;
        desired.x87[0]=(desired.x87[0]&~0xc00u)|(rounding<<10);
        desired.mxcsr=(desired.mxcsr&~0x6000u)|(rounding<<13);
        auto pending=clean; pending.mxcsr|=flags;
        ready.arrive_and_wait();
        Write(pending);
        *__error_nid_postfix()=static_cast<int>(flags);
        const int result=updateEnvironment(&desired);
        const int error=*__error_nid_postfix();
        const auto after=Read();
        ready.arrive_and_wait();
        Write(entry);
        desired.x87[1]|=flags; desired.mxcsr|=flags;
        Require(result==0 && error==static_cast<int>(flags),"thread return/errno");
        Require(Same(after,desired),"thread environment and flag union");
    };
    std::thread a(worker,1,4),b(worker,2,8);a.join();b.join();
    Write(original);
}
volatile std::sig_atomic_t entered=0;
#ifdef _WIN32
unsigned expectedFlag;
DWORD expectedException;
LONG CALLBACK Trap(EXCEPTION_POINTERS* exception) {
    const auto& state=exception->ContextRecord->FltSave;
    const auto active=state.StatusWord&~state.ControlWord&0x3f;
    const auto code=exception->ExceptionRecord->ExceptionCode;
    const bool denormal=expectedFlag==2 && code==EXCEPTION_FLT_INVALID_OPERATION;
    if(entered && active==expectedFlag && (code==expectedException || denormal)) std::_Exit(0);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
void Trap(int){std::_Exit(entered?0:1);}
#endif
void Pending(unsigned bit,unsigned source) {
    Require(bit<6 && source<3,"trap arguments");
#ifdef _WIN32
    constexpr DWORD codes[]{EXCEPTION_FLT_INVALID_OPERATION,EXCEPTION_FLT_DENORMAL_OPERAND,EXCEPTION_FLT_DIVIDE_BY_ZERO,EXCEPTION_FLT_OVERFLOW,EXCEPTION_FLT_UNDERFLOW,EXCEPTION_FLT_INEXACT_RESULT};
    expectedFlag=1u<<bit;expectedException=codes[bit];
    Require(AddVectoredExceptionHandler(1,Trap)!=nullptr,"install trap handler");
#else
    Require(std::signal(SIGFPE,Trap)!=SIG_ERR,"install trap handler");
#endif
    auto desired=Read();
    desired.x87[0]=(desired.x87[0]|0x3f)&~(1u<<bit);
    desired.x87[1]&=~0xffu;
    desired.mxcsr=(desired.mxcsr&~0x3fu)|(0x3f<<7);
    auto pending=desired;pending.x87[0]|=0x3f;
    if(source==0) pending.x87[1]|=1u<<bit;
    else if(source==1) pending.mxcsr|=1u<<bit;
    else desired.x87[1]|=1u<<bit;
    Write(pending);
    entered=1;
    updateEnvironment(&desired);
    entered=0;
    Require(false,"enabled pending exception did not trap during feupdateenv");
}

void DefaultEnvironment(const Environment* fallback) {
    Require(fallback!=nullptr,"__fe_dfl_env is missing");
    constexpr unsigned char measured[32]{0x7f,0x03,0xff,0xff,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xff,0xff,0xc0,0x9f,0x00,0x00};
    Require(std::memcmp(fallback,measured,sizeof(measured))==0,"__fe_dfl_env bytes");
    const auto original=Read();
    auto disturbed=original;
    disturbed.x87[0]=(disturbed.x87[0]&~0xf3fu)|0xc00u|0x3fu;
    disturbed.x87[1]&=~0xffu;
    disturbed.mxcsr=(disturbed.mxcsr&~0x7fffu)|0x6000u|(0x3f<<7);
    Write(disturbed);
    Require(updateEnvironment(fallback)==0,"feupdateenv(__fe_dfl_env) result");
    const auto restored=Read();
    Require((restored.x87[0]&0xffffu)==0x037fu,"__fe_dfl_env x87 control");
    Require((restored.x87[1]&0x3fu)==0 && restored.mxcsr==0x9fc0u,"__fe_dfl_env leaves no flags when none were pending");
    Write(original);
}

int main(int argc,char** argv) {
    Require(argc==2 || argc==4,"expected libc path and optional trap bit/source");
#ifdef _WIN32
    const auto library=LoadLibraryA(argv[1]);
    Require(library!=nullptr,"load libc");
    updateEnvironment=reinterpret_cast<UpdateEnvironment>(GetProcAddress(library,"feupdateenv_nid_postfix"));
#else
    const auto library=dlopen(argv[1],RTLD_NOW);
    Require(library!=nullptr,"load libc");
    updateEnvironment=reinterpret_cast<UpdateEnvironment>(dlsym(library,"feupdateenv_nid_postfix"));
#endif
    Require(updateEnvironment!=nullptr,"export is missing");
#ifdef _WIN32
    const auto fallback=reinterpret_cast<const Environment*>(GetProcAddress(library,"__fe_dfl_env_nid_postfix"));
#else
    const auto fallback=static_cast<const Environment*>(dlsym(library,"__fe_dfl_env_nid_postfix"));
#endif
    if(argc==4) Pending(static_cast<unsigned>(std::atoi(argv[2])),static_cast<unsigned>(std::atoi(argv[3])));
    Matrix(); TagsAndPointers(); BoundsAndErrors(); MasksAndThreads(); DefaultEnvironment(fallback);
#ifdef _WIN32
    Require(FreeLibrary(library)!=0,"release library lookup handle");
#else
    Require(dlclose(library)==0,"release library lookup handle");
#endif
}

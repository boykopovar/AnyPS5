#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <dlfcn.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
using Raise = int (APS5_VABI*)(int);
Raise raiseFlags;

void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr,"feraiseexcept: %s\n",message); std::abort(); }
}

struct Environment { std::array<std::uint32_t,7> x87; std::uint32_t sse; };
Environment Read() {
    Environment state;
    asm volatile("fnstenv %0\n\tfldenv %0" : "=m"(state.x87));
    state.sse=_mm_getcsr();
    return state;
}
void Write(const Environment& state) {
    asm volatile("fldenv %0" : : "m"(state.x87));
    _mm_setcsr(state.sse);
}

void Flags() {
    const auto original=Read();
    for (const unsigned precision: {0u,0x200u,0x300u})
        for (unsigned rounding=0;rounding<16;++rounding)
            for (unsigned old=0;old<64;++old)
                for (unsigned raised=0;raised<64;++raised) {
                    auto before=original;
                    before.x87[0]=(before.x87[0]&~0xf3fu)|0x3f|precision|((rounding&3)<<10);
                    before.x87[1]=(before.x87[1]&~0xffu)|old;
                    before.sse=(before.sse&~0x603fu)|(0x3f<<7)|((rounding>>2)<<13)|(old^0x3f);
                    Write(before);
                    before=Read();
                    *__error_nid_postfix()=123;
                    Require(raiseFlags(static_cast<int>(raised))==0,"return value");
                    const auto after=Read();
                    Require(*__error_nid_postfix()==123,"errno preserved");
                    Require((after.x87[1]&0x3f)==(old|raised),"x87 flags accumulate");
                    Require((after.sse&0x3f)==((old^0x3f)|raised),"SSE flags accumulate independently");
                    Require((after.x87[1]&~0x3fu)==(before.x87[1]&~0x3fu),"other x87 status preserved");
                    Require((after.sse&~0x3fu)==(before.sse&~0x3fu),"SSE controls preserved");
                    for (unsigned word: {0u,2u,3u,4u,5u,6u})
                        Require(after.x87[word]==before.x87[word],"x87 controls, tags and pointers preserved");
                }
    Write(original);
    for (const int invalid: {-1,0x40,0x10000,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        bool rejected=false;
        try { raiseFlags(invalid); } catch (const std::exception&) { rejected=true; }
        Require(rejected,"unverified exception bits rejected");
        const auto after=Read();
        Require(after.x87==original.x87 && after.sse==original.sse,"rejected mask preserves environment");
    }
}

void DifferentMasks() {
    const auto original=Read();
    for (unsigned bit=0;bit<6;++bit) {
        auto seed=original;
        seed.x87[0]|=0x3f;
        seed.x87[1]&=~0xffu;
        seed.sse=(seed.sse&~0x3fu)|(0x3f<<7);
        seed.sse&=~(1u<<(bit+7));
        Write(seed);
        const int result=raiseFlags(static_cast<int>(1u<<bit));
        const auto after=Read();
        Write(original);
        Require(result==0,"SSE-only unmask does not deliver an x87 trap");
        Require((after.x87[1]&0x3f)==(1u<<bit) && (after.sse&0x3f)==(1u<<bit),"flags with differing exception masks");
        Require(after.x87[0]==seed.x87[0] && (after.sse&~0x3fu)==(seed.sse&~0x3fu),"differing masks preserved");
        seed.x87[0]&=~(1u<<bit);
        seed.sse|=0x3f<<7;
        Write(seed);
        const int noFlags=raiseFlags(0);
        const auto unchanged=Read();
        Write(original);
        Require(noFlags==0 && (unchanged.x87[1]&0x3f)==0 && (unchanged.sse&0x3f)==0,"empty mask does not trap with x87 enabled");
    }
}

void Threads() {
    std::barrier ready(2);
    auto worker=[&](unsigned flag) {
        const auto original=Read();
        auto seed=original;
        seed.x87[0]|=0x3f;
        seed.x87[1]&=~0xffu;
        seed.sse=(seed.sse&~0x3fu)|(0x3f<<7);
        Write(seed);
        ready.arrive_and_wait();
        *__error_nid_postfix()=static_cast<int>(100+flag);
        Require(raiseFlags(static_cast<int>(flag))==0,"thread return");
        const int raisedError=*__error_nid_postfix();
        ready.arrive_and_wait();
        const auto after=Read();
        Require((after.x87[1]&0x3f)==flag && (after.sse&0x3f)==flag,"thread-local exception flags");
        Require(raisedError==static_cast<int>(100+flag),"thread-local errno");
        Write(original);
    };
    std::thread a(worker,1),b(worker,4);
    a.join(); b.join();
}

#ifdef _WIN32
DWORD expectedException;
unsigned expectedFlag;
LONG CALLBACK Trap(EXCEPTION_POINTERS* exception) {
    const auto code=exception->ExceptionRecord->ExceptionCode;
    const auto& state=exception->ContextRecord->FltSave;
    const auto active=state.StatusWord & ~state.ControlWord & 0x3f;
    const bool denormalCode=expectedFlag==2 && code==EXCEPTION_FLT_INVALID_OPERATION;
    if ((code==expectedException || denormalCode) && active==expectedFlag) std::_Exit(0);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
void Trap(int) { std::_Exit(0); }
#endif

void CheckTrap(unsigned bit) {
    Require(bit<6,"trap bit");
#ifdef _WIN32
    constexpr DWORD codes[]{EXCEPTION_FLT_INVALID_OPERATION,EXCEPTION_FLT_DENORMAL_OPERAND,EXCEPTION_FLT_DIVIDE_BY_ZERO,EXCEPTION_FLT_OVERFLOW,EXCEPTION_FLT_UNDERFLOW,EXCEPTION_FLT_INEXACT_RESULT};
    expectedException=codes[bit];
    expectedFlag=1u<<bit;
    Require(AddVectoredExceptionHandler(1,Trap)!=nullptr,"install trap handler");
#else
    Require(std::signal(SIGFPE,Trap)!=SIG_ERR,"install trap handler");
#endif
    auto state=Read();
    state.x87[0]=(state.x87[0]|0x3f)&~(1u<<bit);
    state.x87[1]&=~0xffu;
    state.sse=(state.sse&~0x3fu)|(0x3f<<7);
    Write(state);
    raiseFlags(static_cast<int>(1u<<bit));
    Require(false,"enabled x87 exception did not trap");
}

int main(int argc,char** argv) {
    Require(argc==2 || argc==3,"expected library path and optional trap bit");
#ifdef _WIN32
    const auto library=LoadLibraryA(argv[1]);
    Require(library!=nullptr,"load libc");
    raiseFlags=reinterpret_cast<Raise>(GetProcAddress(library,"feraiseexcept_nid_postfix"));
#else
    const auto library=dlopen(argv[1],RTLD_NOW);
    Require(library!=nullptr,"load libc");
    raiseFlags=reinterpret_cast<Raise>(dlsym(library,"feraiseexcept_nid_postfix"));
#endif
    Require(raiseFlags!=nullptr,"feraiseexcept export is missing");
    if (argc==3) CheckTrap(static_cast<unsigned>(std::atoi(argv[2])));
    Flags(); DifferentMasks(); Threads();
#ifdef _WIN32
    Require(FreeLibrary(library)!=0,"release lookup handle");
#else
    Require(dlclose(library)==0,"release lookup handle");
#endif
    std::puts("feraiseexcept checks passed");
}

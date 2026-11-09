#include "prx/libSceRtc/include/Rtc.hpp"
#include <chrono>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

struct Date {
    const char* text;
    int year,month,day,hour,minute,second,microsecond,offset;
    bool generic;
};
static std::uint64_t Tick(const Date& d) {
    using namespace std::chrono;
    const auto date = sys_days{year{d.year}/month{static_cast<unsigned>(d.month)}/day{static_cast<unsigned>(d.day)}};
    const auto epoch = sys_days{year{1}/January/1};
    const std::int64_t days = (date-epoch).count();
    const std::int64_t seconds = days*86400 + d.hour*3600 + d.minute*60 + d.second - d.offset*60;
    return static_cast<std::uint64_t>(seconds)*1000000 + d.microsecond;
}
int main() {
    const Date dates[]{
        {"1970-01-01T00:00:00Z",1970,1,1,0,0,0,0,0,false},
        {"2000-02-29T23:59:59.123456Z",2000,2,29,23,59,59,123456,0,false},
        {"2024-02-29T14:04:56.789+01:30",2024,2,29,14,4,56,789000,90,false},
        {"2024-01-01T00:15:00-00:45",2024,1,1,0,15,0,0,-45,false},
        {"2024-12-31T23:59:59.000001+00:01",2024,12,31,23,59,59,1,1,false},
        {"9998-12-31T23:59:59.999999Z",9998,12,31,23,59,59,999999,0,false},
        {"Thu, 01 Jan 1970 00:00:00 +0000",1970,1,1,0,0,0,0,0,true},
        {"Thu, 29 Feb 2024 14:04:56 +0130",2024,2,29,14,4,56,0,90,true},
        {"Thu, 29 Feb 2024 11:04:56 -0130",2024,2,29,11,4,56,0,-90,true},
        {"Thu Feb 29 12:34:56 2024",2024,2,29,12,34,56,0,0,true}};
    std::size_t cases = 0;
    for (const auto& d : dates) for (bool generic : {false,true}) {
        if (d.generic && !generic) continue;
        const auto length = std::strlen(d.text);
        Guarded input(length+1), output(sizeof(RtcTick));
        auto* text = reinterpret_cast<char*>(input.End(length+1));
        std::memcpy(text,d.text,length+1);
        input.Protect(true);
        auto* tick = std::construct_at(reinterpret_cast<RtcTick*>(output.End(sizeof(RtcTick))));
        const int result = generic ? sceRtcParseDateTime(tick,text) : sceRtcParseRFC3339(tick,text);
        Require(result == 0 && tick->tick == Tick(d));
        Require(std::memcmp(text,d.text,length+1) == 0);
        input.Prefix(text);output.Prefix(tick);
        ++cases;
    }
    std::printf("%zu guarded date parsing input/output cases passed\n", cases);
}

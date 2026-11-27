#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfSystem guest pointer check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename Function>
void RequireNotImplemented(Function&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

template<typename Function>
void RequireInvalidArgument(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestConstantTimeMemcmp() {
    GuardedPages pages;
    const unsigned char a[16]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const unsigned char b[16]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const unsigned char c[16]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 99};

    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(a, b, sizeof(a)) == 0);
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(a, c, sizeof(a)) != 0);
    Require(_ZN3WTF18constantTimeMemcmpEPKvS1_m(pages.Guard(), pages.Guard(), 0) == 0);

    RequireNotImplemented([&] {
        _ZN3WTF18constantTimeMemcmpEPKvS1_m(pages.Guard(), b, 8);
    });
    RequireNotImplemented([&] {
        _ZN3WTF18constantTimeMemcmpEPKvS1_m(a, pages.Guard(), 8);
    });
    RequireNotImplemented([&] {
        _ZN3WTF18constantTimeMemcmpEPKvS1_m(pages.Guard() - 4, b, 8);
    });
    RequireNotImplemented([&] {
        _ZN3WTF18constantTimeMemcmpEPKvS1_m(nullptr, b, 8);
    });
}

void TestRandomValues() {
    GuardedPages pages;
    unsigned char out[32]{};
    _ZN3WTF29cryptographicallyRandomValuesEPvm(out, sizeof(out));
    _ZN3WTF29cryptographicallyRandomValuesEPvm(nullptr, 16);
    _ZN3WTF29cryptographicallyRandomValuesEPvm(pages.Guard(), 0);

    RequireNotImplemented([&] {
        _ZN3WTF29cryptographicallyRandomValuesEPvm(pages.Guard(), 16);
    });
    RequireNotImplemented([&] {
        _ZN3WTF29cryptographicallyRandomValuesEPvm(pages.ReadOnly(), 16);
    });
    RequireNotImplemented([&] {
        _ZN3WTF29cryptographicallyRandomValuesEPvm(pages.ReadOnly() - 4, 16);
    });
}

void TestMappedFileAndFileSystem() {
    GuardedPages pages;
    auto* guardMapped = reinterpret_cast<WTF::MappedFileData*>(pages.Guard());
    auto* roMapped = reinterpret_cast<WTF::MappedFileData*>(pages.ReadOnly());
    auto* guardBool = reinterpret_cast<bool*>(pages.Guard());
    auto* roBool = reinterpret_cast<bool*>(pages.ReadOnly());
    auto* guardInt = reinterpret_cast<int*>(pages.Guard());
    auto* roInt = reinterpret_cast<int*>(pages.ReadOnly());
    const auto* guardString = reinterpret_cast<const String*>(pages.Guard());

    WTF::MappedFileData validMapped{};
    bool ok = true;
    const String nullPath{};

    RequireInvalidArgument([&] {
        _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(nullptr, -1, 0, 0);
    });
    RequireInvalidArgument([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(nullptr, -1, 0, 0, ok);
    });
    RequireInvalidArgument([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(nullptr, nullPath, 0, ok);
    });
    RequireInvalidArgument([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(nullptr);
    });
    RequireInvalidArgument([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataD2Ev(nullptr);
    });

    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(guardMapped, -1, 0, 0);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileData13mapFileHandleEiNS0_12FileOpenModeENS0_14MappedFileModeE(roMapped, -1, 0, 0);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(guardMapped, -1, 0, 0, ok);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(roMapped, -1, 0, 0, ok);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(&validMapped, -1, 0, 0, *guardBool);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1EiNS0_12FileOpenModeENS0_14MappedFileModeERb(&validMapped, -1, 0, 0, *roBool);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC1ERKNS_6StringENS0_14MappedFileModeERb(&validMapped, *guardString, 0, ok);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataC2ERKNS_6StringENS0_14MappedFileModeERb(roMapped, nullPath, 0, ok);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataD1Ev(guardMapped);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl14MappedFileDataD2Ev(guardMapped);
    });

    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl9closeFileERi(*guardInt);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl9closeFileERi(*roInt);
    });

    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(*guardString, 0, true, false);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE(*guardString);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE(*guardString, 0, 0);
    });

    StringImpl badImpl{2, 8, pages.Guard(), 4};
    String badPath;
    badPath.impl = &badImpl;
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(badPath, 0, true, false);
    });
    RequireNotImplemented([&] {
        _ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE(badPath);
    });
    badPath.impl = nullptr;
}

}  // namespace

int main() {
    TestConstantTimeMemcmp();
    TestRandomValues();
    TestMappedFileAndFileSystem();
    return 0;
}

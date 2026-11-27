#include "prx/libc/include/General.hpp"
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

using NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError;
using NKWebKit::JscBytecodeCacheErrorAbi::StandardError;
using NKWebKit::JscBytecodeCacheErrorAbi::WriteError;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSC::BytecodeCacheError check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static BytecodeCacheError Filled(std::uint32_t index) {
    BytecodeCacheError error;
    std::memset(&error, 0xA5, sizeof(error));
    error.index = index;
    return error;
}

static BytecodeCacheError Parser(std::uint8_t type, StringImpl* message) {
    BytecodeCacheError error = Filled(0);
    error.parserError.message = message;
    error.parserError.type = type;
    return error;
}

static bool BytesFrom(const BytecodeCacheError& error, std::size_t begin, std::size_t end, unsigned char value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&error);
    for (std::size_t offset = begin; offset < end; ++offset) {
        if (bytes[offset] != value) return false;
    }
    return true;
}

static bool Same(const BytecodeCacheError& left, const BytecodeCacheError& right) {
    return std::memcmp(&left, &right, sizeof(BytecodeCacheError)) == 0;
}

int main() {
    {
        BytecodeCacheError none = Parser(0, nullptr);
        Require(!_ZNK3JSC18BytecodeCacheError7isValidEv(&none));
        for (std::uint8_t type = 1; type <= 4; ++type) {
            BytecodeCacheError error = Parser(type, nullptr);
            Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));
        }
        BytecodeCacheError standard = Filled(1);
        standard.standardError.error = 0;
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&standard));
        BytecodeCacheError write = Filled(2);
        write.writeError = WriteError{0, 0};
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&write));
        BytecodeCacheError valueless = Filled(0xFFFFFFFFu);
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(&valueless); }));
        BytecodeCacheError invalid = Filled(3);
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(&invalid); }));
        Require(Throws([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(nullptr); }));
    }
    {
        StringImpl message{4, 0, nullptr, 0};
        BytecodeCacheError error = Parser(4, &message);
        StandardError standard{28};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &standard) == &error);
        Require(message.refCount == 2);
        Require(error.index == 1);
        Require(error.standardError.error == 28);
        Require(BytesFrom(error, 4, 72, 0xA5));
        Require(error.parserError.message == &message);
        Require(BytesFrom(error, 80, 84, 0xA5));
        Require(error.parserError.type == 4);
        Require(BytesFrom(error, 85, 88, 0xA5));
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));

        StandardError other{-5};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &other) == &error);
        Require(message.refCount == 2);
        Require(error.index == 1);
        Require(error.standardError.error == -5);

        WriteError write{0x1122334455667788ull, 0x99aabbccddeeff00ull};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &write) == &error);
        Require(message.refCount == 2);
        Require(error.index == 2);
        Require(error.writeError.written == 0x1122334455667788ull);
        Require(error.writeError.expected == 0x99aabbccddeeff00ull);
        Require(BytesFrom(error, 16, 72, 0xA5));
        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(&error));

        WriteError next{7, 9};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &next) == &error);
        Require(error.index == 2);
        Require(error.writeError.written == 7);
        Require(error.writeError.expected == 9);

        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, &standard) == &error);
        Require(error.index == 1);
        Require(error.standardError.error == 28);
        Require(error.writeError.expected == 9);
        Require(message.refCount == 2);
    }
    {
        StringImpl message{6, 0, nullptr, 0};
        BytecodeCacheError error = Parser(0, &message);
        WriteError write{3, 4};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, &write) == &error);
        Require(message.refCount == 4);
        Require(error.index == 2);
        Require(error.writeError.written == 3);
        Require(error.writeError.expected == 4);
    }
    {
        BytecodeCacheError nullMessage = Parser(0, nullptr);
        StandardError standard{1};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&nullMessage, &standard) == &nullMessage);
        Require(nullMessage.index == 1);
        Require(nullMessage.standardError.error == 1);

        BytecodeCacheError valueless = Filled(0xFFFFFFFFu);
        WriteError write{11, 12};
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&valueless, &write) == &valueless);
        Require(valueless.index == 2);
        Require(valueless.writeError.written == 11);
        Require(valueless.writeError.expected == 12);
        Require(BytesFrom(valueless, 16, 88, 0xA5));

        BytecodeCacheError valuelessStandard = Filled(0xFFFFFFFFu);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&valuelessStandard, &standard) == &valuelessStandard);
        Require(valuelessStandard.index == 1);
        Require(valuelessStandard.standardError.error == 1);
        Require(BytesFrom(valuelessStandard, 4, 88, 0xA5));
    }
    {
        StandardError standard{2};
        WriteError write{1, 2};
        BytecodeCacheError invalid = Filled(3);
        BytecodeCacheError before = invalid;
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&invalid, &standard); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&invalid, &write); }));
        Require(Same(invalid, before));
        BytecodeCacheError error = Parser(0, nullptr);
        BytecodeCacheError original = error;
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&error, nullptr); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&error, nullptr); }));
        Require(Same(error, original));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(nullptr, &standard); }));
        Require(Throws([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(nullptr, &write); }));
    }
    {
        struct GuardedPages {
            unsigned char* base = nullptr;
            std::size_t pageSize = 0;

            GuardedPages() {
#ifdef _WIN32
                SYSTEM_INFO info{};
                GetSystemInfo(&info);
                pageSize = info.dwPageSize;
                base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                Require(base != nullptr);
                DWORD previous = 0;
                Require(VirtualProtect(base + pageSize * 3, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
                const long size = sysconf(_SC_PAGESIZE);
                Require(size > 0);
                pageSize = static_cast<std::size_t>(size);
                void* mapped = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                Require(mapped != MAP_FAILED);
                base = static_cast<unsigned char*>(mapped);
                Require(mprotect(base + pageSize * 3, pageSize, PROT_NONE) == 0);
#endif
            }

            ~GuardedPages() {
                if (!base) return;
#ifdef _WIN32
                VirtualFree(base, 0, MEM_RELEASE);
#else
                munmap(base, pageSize * 4);
#endif
            }

            void ProtectReadOnly() {
#ifdef _WIN32
                DWORD previous = 0;
                Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
#else
                Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
#endif
            }

            unsigned char* ReadOnly() const { return base + pageSize; }
            unsigned char* Guard() const { return base + pageSize * 3; }
        };

        auto requireNotImplemented = [](auto&& function) {
            const auto before = NotImplementedCalls_nid_no_patch();
            bool threw = false;
            try {
                function();
            } catch (const std::runtime_error&) {
                threw = true;
            }
            Require(threw);
            Require(NotImplementedCalls_nid_no_patch() == before + 1);
        };

        GuardedPages pages;
        BytecodeCacheError roStandard = Filled(1);
        roStandard.standardError.error = 17;
        StandardError roErr{42};
        WriteError roWrite{10, 20};
        StringImpl roMessage{4, 0, nullptr, 0};
        std::memcpy(pages.ReadOnly(), &roStandard, sizeof(roStandard));
        std::memcpy(pages.ReadOnly() + 128, &roErr, sizeof(roErr));
        std::memcpy(pages.ReadOnly() + 160, &roWrite, sizeof(roWrite));
        std::memcpy(pages.ReadOnly() + 192, &roMessage, sizeof(roMessage));
        pages.ProtectReadOnly();

        Require(_ZNK3JSC18BytecodeCacheError7isValidEv(reinterpret_cast<const BytecodeCacheError*>(pages.ReadOnly())));
        BytecodeCacheError validTarget = Filled(1);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&validTarget, reinterpret_cast<const StandardError*>(pages.ReadOnly() + 128)) == &validTarget);
        Require(validTarget.standardError.error == 42);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&validTarget, reinterpret_cast<const WriteError*>(pages.ReadOnly() + 160)) == &validTarget);
        Require(validTarget.writeError.written == 10 && validTarget.writeError.expected == 20);

        requireNotImplemented([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(reinterpret_cast<const BytecodeCacheError*>(pages.Guard())); });
        requireNotImplemented([&] { _ZNK3JSC18BytecodeCacheError7isValidEv(reinterpret_cast<const BytecodeCacheError*>(pages.Guard() - (sizeof(BytecodeCacheError) - 4))); });

        StandardError standard{5};
        WriteError write{7, 8};
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(reinterpret_cast<BytecodeCacheError*>(pages.Guard()), &standard); });
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(reinterpret_cast<BytecodeCacheError*>(pages.ReadOnly()), &standard); });
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(reinterpret_cast<BytecodeCacheError*>(pages.Guard()), &write); });
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(reinterpret_cast<BytecodeCacheError*>(pages.ReadOnly()), &write); });

        unsigned char* const splitTarget = pages.ReadOnly() - 8;
        std::memset(splitTarget, 0x5A, 8);
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(reinterpret_cast<BytecodeCacheError*>(splitTarget), &standard); });
        for (std::size_t i = 0; i < 8; ++i) Require(splitTarget[i] == 0x5A);
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(reinterpret_cast<BytecodeCacheError*>(splitTarget), &write); });
        for (std::size_t i = 0; i < 8; ++i) Require(splitTarget[i] == 0x5A);

        StringImpl liveMsg{4, 0, nullptr, 0};
        BytecodeCacheError parserWithLive = Parser(2, &liveMsg);
        const BytecodeCacheError parserOriginal = parserWithLive;
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithLive, reinterpret_cast<const StandardError*>(pages.Guard())); });
        Require(liveMsg.refCount == 4 && Same(parserWithLive, parserOriginal));
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithLive, reinterpret_cast<const WriteError*>(pages.Guard())); });
        Require(liveMsg.refCount == 4 && Same(parserWithLive, parserOriginal));

        BytecodeCacheError parserWithGuardMsg = Parser(2, reinterpret_cast<StringImpl*>(pages.Guard()));
        const BytecodeCacheError guardMsgOriginal = parserWithGuardMsg;
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithGuardMsg, &standard); });
        Require(Same(parserWithGuardMsg, guardMsgOriginal));
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithGuardMsg, &write); });
        Require(Same(parserWithGuardMsg, guardMsgOriginal));

        BytecodeCacheError parserWithRoMsg = Parser(2, reinterpret_cast<StringImpl*>(pages.ReadOnly() + 192));
        const BytecodeCacheError roMsgOriginal = parserWithRoMsg;
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithRoMsg, &standard); });
        Require(Same(parserWithRoMsg, roMsgOriginal));
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithRoMsg, &write); });
        Require(Same(parserWithRoMsg, roMsgOriginal));

        struct SubstringBuffer {
            StringImpl header;
            StringImpl* owner;
        };
        static_assert(sizeof(SubstringBuffer) == 32);

        SubstringBuffer guardOwnerSub{StringImpl{2, 0, nullptr, 2}, reinterpret_cast<StringImpl*>(pages.Guard())};
        BytecodeCacheError parserWithGuardOwner = Parser(2, &guardOwnerSub.header);
        const BytecodeCacheError guardOwnerOriginal = parserWithGuardOwner;
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithGuardOwner, &standard); });
        Require(Same(parserWithGuardOwner, guardOwnerOriginal));
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithGuardOwner, &write); });
        Require(Same(parserWithGuardOwner, guardOwnerOriginal));

        unsigned char* const truncatedSubAddr = pages.Guard() - 24;
        const StringImpl truncatedSubHeader{2, 0, nullptr, 2};
        std::memcpy(truncatedSubAddr, &truncatedSubHeader, sizeof(truncatedSubHeader));
        BytecodeCacheError parserWithTruncatedSub = Parser(2, reinterpret_cast<StringImpl*>(truncatedSubAddr));
        const BytecodeCacheError truncatedSubOriginal = parserWithTruncatedSub;
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithTruncatedSub, &standard); });
        Require(Same(parserWithTruncatedSub, truncatedSubOriginal));
        requireNotImplemented([&] { _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithTruncatedSub, &write); });
        Require(Same(parserWithTruncatedSub, truncatedSubOriginal));

        SubstringBuffer sharedRefSub{StringImpl{4, 0, nullptr, 2}, reinterpret_cast<StringImpl*>(pages.Guard())};
        BytecodeCacheError parserWithSharedRef = Parser(2, &sharedRefSub.header);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithSharedRef, &standard) == &parserWithSharedRef);
        Require(parserWithSharedRef.index == 1 && parserWithSharedRef.standardError.error == 5 && sharedRefSub.header.refCount == 2);

        SubstringBuffer sharedRefSubWrite{StringImpl{4, 0, nullptr, 2}, reinterpret_cast<StringImpl*>(pages.Guard())};
        BytecodeCacheError parserWithSharedRefWrite = Parser(2, &sharedRefSubWrite.header);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithSharedRefWrite, &write) == &parserWithSharedRefWrite);
        Require(parserWithSharedRefWrite.index == 2 && parserWithSharedRefWrite.writeError.written == 7 && parserWithSharedRefWrite.writeError.expected == 8 && sharedRefSubWrite.header.refCount == 2);

        StringImpl liveOwner{4, 0, nullptr, 0};
        auto* const heapSubStandard = static_cast<SubstringBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SubstringBuffer)));
        heapSubStandard->header = StringImpl{2, 0, nullptr, 2};
        heapSubStandard->owner = &liveOwner;
        BytecodeCacheError parserWithLiveSub = Parser(2, &heapSubStandard->header);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(&parserWithLiveSub, &standard) == &parserWithLiveSub);
        Require(parserWithLiveSub.index == 1 && parserWithLiveSub.standardError.error == 5 && liveOwner.refCount == 2);

        StringImpl liveOwnerWrite{4, 0, nullptr, 0};
        auto* const heapSubWrite = static_cast<SubstringBuffer*>(_ZN3WTF10fastMallocEm(sizeof(SubstringBuffer)));
        heapSubWrite->header = StringImpl{2, 0, nullptr, 2};
        heapSubWrite->owner = &liveOwnerWrite;
        BytecodeCacheError parserWithLiveSubWrite = Parser(2, &heapSubWrite->header);
        Require(_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(&parserWithLiveSubWrite, &write) == &parserWithLiveSubWrite);
        Require(parserWithLiveSubWrite.index == 2 && parserWithLiveSubWrite.writeError.written == 7 && parserWithLiveSubWrite.writeError.expected == 8 && liveOwnerWrite.refCount == 2);
    }
    return 0;
}

#include "prx/libcurl/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include "prx/libc/include/Api.hpp"

#include <array>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <cstdarg>
#include <memory>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#define CURL_ARGS_START(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define CURL_ARG(type) __builtin_va_arg(args, type)
#define CURL_ARGS_END() __builtin_sysv_va_end(args)
#else
#define CURL_ARGS_START(last) std::va_list args; va_start(args, last)
#define CURL_ARG(type) va_arg(args, type)
#define CURL_ARGS_END() va_end(args)
#endif

extern "C" {
extern curl_malloc_callback Curl_cmalloc;
extern curl_free_callback Curl_cfree;
extern curl_realloc_callback Curl_crealloc;
extern curl_strdup_callback Curl_cstrdup;
extern curl_calloc_callback Curl_ccalloc;
}

namespace {

void* GuestCurlMalloc(size_t size) {
    return malloc_nid_postfix(size);
}

void GuestCurlFree(void* pointer) {
    free_nid_postfix(pointer);
}

void* GuestCurlRealloc(void* pointer, size_t size) {
    return realloc_nid_postfix(pointer, size);
}

char* GuestCurlStrdup(const char* text) {
    return strdup_nid_postfix(text);
}

void* GuestCurlCalloc(size_t count, size_t size) {
    return calloc_nid_postfix(count, size);
}

const bool guestAllocatorInstalled = [] {
    Curl_cmalloc = GuestCurlMalloc;
    Curl_cfree = GuestCurlFree;
    Curl_crealloc = GuestCurlRealloc;
    Curl_cstrdup = GuestCurlStrdup;
    Curl_ccalloc = GuestCurlCalloc;
    return true;
}();

std::mutex globalMutex;
unsigned globalInitCount = 0;

CURLcode GlobalInitWithGuestAllocator(long flags) {
    const CURLcode result = ::curl_global_init_mem(flags, GuestCurlMalloc, GuestCurlFree, GuestCurlRealloc, GuestCurlStrdup, GuestCurlCalloc);
    if (result == CURLE_OK) ++globalInitCount;
    return result;
}

}

namespace {


long HostLong(int64_t value) {
    if (value < LONG_MIN || value > LONG_MAX) throw std::out_of_range("libcurl guest long");
    return static_cast<long>(value);
}

struct CallbackSlot {
    void* function = nullptr;
    void* data = nullptr;
};

struct EasyContext {
    std::array<CallbackSlot, 314> slots{};
    void* errorStream = &_Stderr_nid_postfix;
    EasyContext() {
        slots[1].data = &_Stdout_nid_postfix;
        slots[9].data = __stdinp_nid_postfix;
    }
};

std::mutex contextMutex;
std::unordered_map<CURL*, std::shared_ptr<EasyContext>> easies;

template<class T>
struct GuestFunction;

template<class R, class... A>
struct GuestFunction<R (*)(A...)> {
    using Type = R (APS5_VABI*)(A...);
};

template<class T, class... A>
auto CallGuest(CallbackSlot& slot, A... args) {
    return reinterpret_cast<typename GuestFunction<T>::Type>(slot.function)(args...);
}

size_t Write(char* buffer, size_t size, size_t count, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[1];
    return slot.function ? CallGuest<curl_write_callback>(slot, buffer, size, count, slot.data)
        : fwrite_nid_postfix(buffer, size, count, static_cast<FileStream*>(slot.data));
}

size_t Read(char* buffer, size_t size, size_t count, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[9];
    return slot.function ? CallGuest<curl_read_callback>(slot, buffer, size, count, slot.data)
        : fread_nid_postfix(buffer, size, count, static_cast<FileStream*>(slot.data));
}

size_t Header(char* buffer, size_t size, size_t count, void* opaque) {
    auto* context = static_cast<EasyContext*>(opaque);
    auto& slot = context->slots[29];
    if (slot.function) return CallGuest<curl_write_callback>(slot, buffer, size, count, slot.data);
    auto& writer = context->slots[1];
    return writer.function ? CallGuest<curl_write_callback>(writer, buffer, size, count, slot.data)
        : fwrite_nid_postfix(buffer, size, count, static_cast<FileStream*>(slot.data));
}

int Debug(CURL* easy, curl_infotype type, char* buffer, size_t size, void* opaque) {
    auto* context = static_cast<EasyContext*>(opaque);
    auto& slot = context->slots[95];
    if (slot.function) return CallGuest<curl_debug_callback>(slot, easy, type, buffer, size, slot.data);
    if (type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT) {
        const char* prefix[] = {"* ", "< ", "> "};
        auto* stream = static_cast<FileStream*>(context->errorStream);
        fwrite_nid_postfix(prefix[type], 2, 1, stream);
        fwrite_nid_postfix(buffer, size, 1, stream);
    }
    return 0;
}

void BindSeek(CURL* easy, EasyContext* context);

void BindEasy(CURL* easy, EasyContext* context) {
    BindSeek(easy, context);
    ::curl_easy_setopt(easy, CURLOPT_WRITEDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, Write);
    ::curl_easy_setopt(easy, CURLOPT_READDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_READFUNCTION, Read);
    ::curl_easy_setopt(easy, CURLOPT_DEBUGDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_DEBUGFUNCTION, Debug);
}

void BindHeader(CURL* easy, EasyContext* context) {
    const auto& slot = context->slots[29];
    ::curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, slot.function || slot.data ? Header : nullptr);
    ::curl_easy_setopt(easy, CURLOPT_HEADERDATA, slot.function || slot.data ? context : nullptr);
}

std::shared_ptr<EasyContext> FindEasy(CURL* easy) {
    std::lock_guard lock(contextMutex);
    const auto found = easies.find(easy);
    return found == easies.end() ? nullptr : found->second;
}

int Progress(void* opaque, double a, double b, double c, double d) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_PROGRESSFUNCTION % 10000];
    return CallGuest<curl_progress_callback>(slot, slot.data, a, b, c, d);
}

int Xfer(void* opaque, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_XFERINFOFUNCTION % 10000];
    return CallGuest<curl_xferinfo_callback>(slot, slot.data, a, b, c, d);
}

int Seek(void* opaque, curl_off_t offset, int origin) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_SEEKFUNCTION % 10000];
    return CallGuest<curl_seek_callback>(slot, slot.data, offset, origin);
}

curlioerr Ioctl(CURL* easy, int command, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_IOCTLFUNCTION % 10000];
    return CallGuest<curl_ioctl_callback>(slot, easy, command, slot.data);
}

int DefaultSeek(void* opaque, curl_off_t offset, int origin) {
    auto* context = static_cast<EasyContext*>(opaque);
    if (fseek_nid_postfix(static_cast<FileStream*>(context->slots[9].data), offset, origin) == -1)
        return CURL_SEEKFUNC_CANTSEEK;
    return CURL_SEEKFUNC_OK;
}

void BindSeek(CURL* easy, EasyContext* context) {
    auto callback = context->slots[167].function ? Seek :
        context->slots[9].function || context->slots[130].function ? nullptr : DefaultSeek;
    ::curl_easy_setopt(easy, CURLOPT_SEEKFUNCTION, callback);
    ::curl_easy_setopt(easy, CURLOPT_SEEKDATA, context);
}

size_t Interleave(char* buffer, size_t size, size_t count, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_INTERLEAVEFUNCTION % 10000];
    return CallGuest<curl_write_callback>(slot, buffer, size, count, slot.data);
}

int Fnmatch(void* opaque, const char* pattern, const char* name) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_FNMATCH_FUNCTION % 10000];
    return CallGuest<curl_fnmatch_callback>(slot, slot.data, pattern, name);
}

int Resolver(void* resolver, void* reserved, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_RESOLVER_START_FUNCTION % 10000];
    return CallGuest<curl_resolver_start_callback>(slot, resolver, reserved, slot.data);
}

int Trailer(curl_slist** list, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_TRAILERFUNCTION % 10000];
    return CallGuest<curl_trailer_callback>(slot, list, slot.data);
}

int Prereq(void* opaque, char* primary, char* local, int primaryPort, int localPort) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_PREREQFUNCTION % 10000];
    return CallGuest<curl_prereq_callback>(slot, slot.data, primary, local, primaryPort, localPort);
}

int SshKey(CURL* easy, const curl_khkey* known, const curl_khkey* found, curl_khmatch match, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[CURLOPT_SSH_KEYFUNCTION % 10000];
    return CallGuest<curl_sshkeycallback>(slot, easy, known, found, match, slot.data);
}

struct GuestHstsEntry {
    char* name;
    size_t namelen;
    unsigned char includeSubDomains;
    char expire[18];
};
static_assert(offsetof(GuestHstsEntry, expire) == 17);
static_assert(sizeof(GuestHstsEntry) == 40);

CURLSTScode HstsRead(CURL* easy, curl_hstsentry* entry, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[301];
    GuestHstsEntry guest{entry->name, entry->namelen, static_cast<unsigned char>(entry->includeSubDomains), {}};
    std::memcpy(guest.expire, entry->expire, 18);
    using Function = CURLSTScode (APS5_VABI*)(CURL*, GuestHstsEntry*, void*);
    auto result = reinterpret_cast<Function>(slot.function)(easy, &guest, slot.data);
    entry->name = guest.name;
    entry->namelen = guest.namelen;
    entry->includeSubDomains = guest.includeSubDomains & 1;
    std::memcpy(entry->expire, guest.expire, 18);
    return result;
}

CURLSTScode HstsWrite(CURL* easy, curl_hstsentry* entry, curl_index* index, void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[303];
    GuestHstsEntry guest{entry->name, entry->namelen, static_cast<unsigned char>(entry->includeSubDomains), {}};
    std::memcpy(guest.expire, entry->expire, 18);
    using Function = CURLSTScode (APS5_VABI*)(CURL*, GuestHstsEntry*, curl_index*, void*);
    auto result = reinterpret_cast<Function>(slot.function)(easy, &guest, index, slot.data);
    entry->name = guest.name;
    entry->namelen = guest.namelen;
    entry->includeSubDomains = guest.includeSubDomains & 1;
    std::memcpy(entry->expire, guest.expire, 18);
    return result;
}

struct GuestFileInfo {
    char* filename;
    curlfiletype filetype;
    int64_t time;
    unsigned int perm;
    int uid;
    int gid;
    curl_off_t size;
    int64_t hardlinks;
    decltype(curl_fileinfo::strings) strings;
    unsigned int flags;
    char* buffer;
    size_t bufferSize;
    size_t bufferUsed;
};
static_assert(sizeof(GuestFileInfo) == 128);

long ChunkBegin(const void* info, void* opaque, int remains) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[198];
    const auto* host = static_cast<const curl_fileinfo*>(info);
    GuestFileInfo guest{host->filename, host->filetype, host->time, host->perm, host->uid, host->gid,
        host->size, host->hardlinks, host->strings, host->flags, host->b_data, host->b_size, host->b_used};
    using Function = int64_t (APS5_VABI*)(const void*, void*, int);
    return HostLong(reinterpret_cast<Function>(slot.function)(&guest, slot.data, remains));
}

long ChunkEnd(void* opaque) {
    auto& slot = static_cast<EasyContext*>(opaque)->slots[199];
    using Function = int64_t (APS5_VABI*)(void*);
    return HostLong(reinterpret_cast<Function>(slot.function)(slot.data));
}

CURLcode SetEasyPointer(CURL* easy, CURLoption option, void* value, EasyContext* context) {
    switch (option) {
    case CURLOPT_WRITEFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, Write);
        if (result == CURLE_OK) context->slots[1].function = value;
        return result;
    }
    case CURLOPT_READFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, Read);
        if (result == CURLE_OK) {
            context->slots[9].function = value;
            BindSeek(easy, context);
        }
        return result;
    }
    case CURLOPT_DEBUGFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, Debug);
        if (result == CURLE_OK) context->slots[95].function = value;
        return result;
    }
    case CURLOPT_HEADERFUNCTION:
        context->slots[29].function = value;
        BindHeader(easy, context);
        return CURLE_OK;
    case CURLOPT_STDERR: {
        auto* stream = value ? static_cast<FileStream*>(value) : &_Stderr_nid_postfix;
        auto result = ::curl_easy_setopt(easy, option, GetNativeStream(stream));
        if (result == CURLE_OK) context->errorStream = stream;
        return result;
    }
    case CURLOPT_PROGRESSFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Progress : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_XFERINFODATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_XFERINFODATA, context);
        }
        return result;
    }
    case CURLOPT_XFERINFOFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Xfer : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_XFERINFODATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_XFERINFODATA, context);
        }
        return result;
    }
    case CURLOPT_SEEKFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Seek : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_SEEKDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_SEEKDATA, context);
        }
        if (result == CURLE_OK) BindSeek(easy, context);
        return result;
    }
    case CURLOPT_IOCTLFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Ioctl : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_IOCTLDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_IOCTLDATA, context);
        }
        if (result == CURLE_OK) BindSeek(easy, context);
        return result;
    }
    case CURLOPT_INTERLEAVEFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Interleave : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_INTERLEAVEDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_INTERLEAVEDATA, context);
        }
        return result;
    }
    case CURLOPT_FNMATCH_FUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Fnmatch : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_FNMATCH_DATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_FNMATCH_DATA, context);
        }
        return result;
    }
    case CURLOPT_RESOLVER_START_FUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Resolver : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_RESOLVER_START_DATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_RESOLVER_START_DATA, context);
        }
        return result;
    }
    case CURLOPT_TRAILERFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Trailer : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_TRAILERDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_TRAILERDATA, context);
        }
        return result;
    }
    case CURLOPT_PREREQFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? Prereq : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_PREREQDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_PREREQDATA, context);
        }
        return result;
    }
    case CURLOPT_SSH_KEYFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? SshKey : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_SSH_KEYDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_SSH_KEYDATA, context);
        }
        return result;
    }
    case CURLOPT_HSTSREADFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? HstsRead : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_HSTSREADDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_HSTSREADDATA, context);
        }
        return result;
    }
    case CURLOPT_HSTSWRITEFUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? HstsWrite : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_HSTSWRITEDATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_HSTSWRITEDATA, context);
        }
        return result;
    }
    case CURLOPT_CHUNK_BGN_FUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? ChunkBegin : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_CHUNK_DATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_CHUNK_DATA, context);
        }
        return result;
    }
    case CURLOPT_CHUNK_END_FUNCTION: {
        auto result = ::curl_easy_setopt(easy, option, value ? ChunkEnd : nullptr);
        if (result == CURLE_OK) {
            auto& slot = context->slots[option % 10000];
            slot.function = value;
            slot.data = context->slots[CURLOPT_CHUNK_DATA % 10000].data;
            ::curl_easy_setopt(easy, CURLOPT_CHUNK_DATA, context);
        }
        return result;
    }
    case CURLOPT_WRITEDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_READDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_HEADERDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            BindHeader(easy, context);
        }
        return result;
    }
    case CURLOPT_DEBUGDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_XFERINFODATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_PROGRESSFUNCTION % 10000].data = value;
            context->slots[CURLOPT_XFERINFOFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_SEEKDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_SEEKFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_IOCTLDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_IOCTLFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_INTERLEAVEDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_INTERLEAVEFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_FNMATCH_DATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_FNMATCH_FUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_RESOLVER_START_DATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_RESOLVER_START_FUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_TRAILERDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_TRAILERFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_PREREQDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_PREREQFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_SSH_KEYDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_SSH_KEYFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_HSTSREADDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_HSTSREADFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_HSTSWRITEDATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_HSTSWRITEFUNCTION % 10000].data = value;
        }
        return result;
    }
    case CURLOPT_CHUNK_DATA: {
        auto result = ::curl_easy_setopt(easy, option, context);
        if (result == CURLE_OK) {
            context->slots[option % 10000].data = value;
            context->slots[CURLOPT_CHUNK_BGN_FUNCTION % 10000].data = value;
            context->slots[CURLOPT_CHUNK_END_FUNCTION % 10000].data = value;
        }
        return result;
    }
    default:
        return ::curl_easy_setopt(easy, option, value);
    }
}

void RebindEasy(CURL* easy, EasyContext* context) {
    BindEasy(easy, context);
    BindHeader(easy, context);
    ::curl_easy_setopt(easy, CURLOPT_XFERINFODATA, context);
    ::curl_easy_setopt(easy, CURLOPT_SEEKDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_IOCTLDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_INTERLEAVEDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_CHUNK_DATA, context);
    ::curl_easy_setopt(easy, CURLOPT_FNMATCH_DATA, context);
    ::curl_easy_setopt(easy, CURLOPT_RESOLVER_START_DATA, context);
    ::curl_easy_setopt(easy, CURLOPT_TRAILERDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_PREREQDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_HSTSREADDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_HSTSWRITEDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_SSH_KEYDATA, context);
}

struct MultiContext {
    void* timer = nullptr;
    void* timerData = nullptr;
};
std::unordered_map<CURLM*, std::shared_ptr<MultiContext>> multis;

int Timer(CURLM* multi, long timeout, void* opaque) {
    auto* context = static_cast<MultiContext*>(opaque);
    using Function = int (APS5_VABI*)(CURLM*, int64_t, void*);
    return reinterpret_cast<Function>(context->timer)(multi, timeout, context->timerData);
}

struct ShareContext {
    guest_curl_lock_callback lock = nullptr;
    guest_curl_unlock_callback unlock = nullptr;
    void* data = nullptr;
};

std::mutex shareMutex;
std::unordered_map<CURLSH*, std::shared_ptr<ShareContext>> shares;

void ShareLock(CURL* easy, curl_lock_data data, curl_lock_access access, void* opaque) {
    auto* context = static_cast<ShareContext*>(opaque);
    context->lock(easy, data, access, context->data);
}

void ShareUnlock(CURL* easy, curl_lock_data data, void* opaque) {
    auto* context = static_cast<ShareContext*>(opaque);
    context->unlock(easy, data, context->data);
}

std::shared_ptr<ShareContext> FindShare(CURLSH* share) {
    std::lock_guard lock(shareMutex);
    const auto found = shares.find(share);
    return found == shares.end() ? nullptr : found->second;
}

} // namespace

extern "C" {

char* APS5_VABI curl_pushheader_bynum_nid_postfix(struct curl_pushheaders* h, size_t num) {
    return ::curl_pushheader_bynum(h, num);
}

char* APS5_VABI curl_pushheader_byname_nid_postfix(struct curl_pushheaders* h, const char* name) {
    return ::curl_pushheader_byname(h, name);
}

CURLFORMcode APS5_VABI curl_formadd_nid_postfix(struct guest_curl_httppost** httppost, struct guest_curl_httppost** last_post, ...) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_formget_nid_postfix(struct guest_curl_httppost* form, void* arg, guest_curl_formget_callback append) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI curl_formfree_nid_postfix(struct guest_curl_httppost* form) {
    NotImplemented_nid_no_patch(__func__);
}

CURLMcode APS5_VABI curl_multi_socket_action_nid_postfix(CURLM* multi_handle, int s, int ev_bitmask, int* running_handles) {
    NotImplemented_nid_no_patch(__func__);
}

CURLMcode APS5_VABI curl_multi_socket_all_nid_postfix(CURLM* multi_handle, int* running_handles) {
    return ::curl_multi_socket_all(multi_handle, running_handles);
}

CURLMcode APS5_VABI curl_multi_socket_nid_postfix(CURLM* multi_handle, int s, int* running_handles) {
    NotImplemented_nid_no_patch(__func__);
}

CURLMcode APS5_VABI curl_multi_assign_nid_postfix(CURLM* multi_handle, int sockfd, void* sockptr) {
    NotImplemented_nid_no_patch(__func__);
}

CURLcode APS5_VABI curl_easy_recv_nid_postfix(CURL* curl, void* buffer, size_t buflen, size_t* n) {
    GuestArena::HostWrite write(buffer, buflen);
    return ::curl_easy_recv(curl, buffer, buflen, n);
}

CURLcode APS5_VABI curl_easy_send_nid_postfix(CURL* curl, const void* buffer, size_t buflen, size_t* n) {
    return ::curl_easy_send(curl, buffer, buflen, n);
}

void APS5_VABI curl_mime_free_nid_postfix(curl_mime* mime) {
    ::curl_mime_free(mime);
}

char* APS5_VABI curl_escape_nid_postfix(const char* string, int length) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_easy_escape_nid_postfix(CURL* handle, const char* string, int length) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_unescape_nid_postfix(const char* string, int length) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_easy_unescape_nid_postfix(CURL* handle, const char* string, int inlength, int* outlength) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvfprintf_nid_postfix(std::FILE* whereto, const char* format, VaList* args) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mfprintf_nid_postfix(std::FILE* whereto, const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvsprintf_nid_postfix(char* buffer, const char* format, VaList* args) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_msprintf_nid_postfix(char* buffer, const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvsnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, VaList* args) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_msnprintf_nid_postfix(char* buffer, size_t maxlength, const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mvprintf_nid_postfix(const char* format, VaList* args) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_mprintf_nid_postfix(const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_mvaprintf_nid_postfix(const char* format, VaList* args) {
    NotImplemented_nid_no_patch(__func__);
}

char* APS5_VABI curl_maprintf_nid_postfix(const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
}

CURLSHcode APS5_VABI curl_share_cleanup_nid_postfix(CURLSH* share_handle) {
    auto context = FindShare(share_handle);
    const auto result = ::curl_share_cleanup(share_handle);
    if (result == CURLSHE_OK) {
        std::lock_guard lock(shareMutex);
        const auto found = shares.find(share_handle);
        if (found != shares.end() && found->second == context) shares.erase(found);
    }
    return result;
}

CURLcode APS5_VABI curl_easy_perform_nid_postfix(CURL* handle) {
    return ::curl_easy_perform(handle);
}

CURLMcode APS5_VABI curl_multi_remove_handle_nid_postfix(CURLM* multi_handle, CURL* easy_handle) {
    return ::curl_multi_remove_handle(multi_handle, easy_handle);
}

CURLsslset APS5_VABI curl_global_sslset_nid_postfix(curl_sslbackend id, const char* name, const guest_curl_ssl_backend*** avail) {
    return ::curl_global_sslset(id, name, reinterpret_cast<const curl_ssl_backend***>(avail));
}

CURLMcode APS5_VABI curl_multi_perform_nid_postfix(CURLM* multi_handle, int* running_handles) {
    return ::curl_multi_perform(multi_handle, running_handles);
}

char* APS5_VABI curl_version_nid_postfix() {
    return ::curl_version();
}

void APS5_VABI curl_free_nid_postfix(void* p) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_strequal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI curl_strnequal() {
    NotImplemented_nid_no_patch(__func__);
}

CURLUcode APS5_VABI curl_url_get_nid_postfix(CURLU* handle, CURLUPart what, char** part, unsigned int flags) {
    return ::curl_url_get(handle, what, part, flags);
}

CURLUcode APS5_VABI curl_url_set_nid_postfix(CURLU* handle, CURLUPart what, const char* part, unsigned int flags) {
    return ::curl_url_set(handle, what, part, flags);
}

CURLU* APS5_VABI curl_url_nid_postfix() {
    return ::curl_url();
}

void APS5_VABI curl_url_cleanup_nid_postfix(CURLU* handle) {
    ::curl_url_cleanup(handle);
}

CURLU* APS5_VABI curl_url_dup_nid_postfix(CURLU* in) {
    return ::curl_url_dup(in);
}

const char* APS5_VABI curl_multi_strerror_nid_postfix(CURLMcode error) {
    return ::curl_multi_strerror(error);
}

const char* APS5_VABI curl_easy_strerror_nid_postfix(CURLcode error) {
    return ::curl_easy_strerror(error);
}

const char* APS5_VABI curl_share_strerror_nid_postfix(CURLSHcode error) {
    return ::curl_share_strerror(error);
}

CURLcode APS5_VABI curl_easy_setopt_nid_postfix(CURL* handle, CURLoption option, ...) {
    if (!handle) return CURLE_BAD_FUNCTION_ARGUMENT;
    switch (option) {
    case CURLOPT_HTTPPOST:
        if constexpr (sizeof(long) != 8) throw std::invalid_argument("CURLOPT_HTTPPOST");
        break;
    case CURLOPT_SSL_CTX_FUNCTION: throw std::invalid_argument("CURLOPT_SSL_CTX_FUNCTION");
    case CURLOPT_SSL_CTX_DATA: throw std::invalid_argument("CURLOPT_SSL_CTX_DATA");
    case CURLOPT_CONV_FROM_NETWORK_FUNCTION: throw std::invalid_argument("CURLOPT_CONV_FROM_NETWORK_FUNCTION");
    case CURLOPT_CONV_TO_NETWORK_FUNCTION: throw std::invalid_argument("CURLOPT_CONV_TO_NETWORK_FUNCTION");
    case CURLOPT_CONV_FROM_UTF8_FUNCTION: throw std::invalid_argument("CURLOPT_CONV_FROM_UTF8_FUNCTION");
    case CURLOPT_SOCKOPTFUNCTION:
    case CURLOPT_OPENSOCKETFUNCTION:
    case CURLOPT_CLOSESOCKETFUNCTION:
    case CURLOPT_SOCKOPTDATA:
    case CURLOPT_OPENSOCKETDATA:
    case CURLOPT_CLOSESOCKETDATA:
        NotImplemented_nid_no_patch(__func__);
    default: break;
    }
    if (!::curl_easy_option_by_id(option)) return ::curl_easy_setopt(handle, option);
    auto context = FindEasy(handle);
    CURL_ARGS_START(option);
    CURLcode result;
    switch (static_cast<int>(option) / 10000) {
    case 0: {
        auto value = CURL_ARG(int64_t);
        CURL_ARGS_END();
        return ::curl_easy_setopt(handle, option, HostLong(value));
    }
    case 3: result = ::curl_easy_setopt(handle, option, CURL_ARG(curl_off_t)); break;
    case 1:
    case 2:
    case 4: result = SetEasyPointer(handle, option, CURL_ARG(void*), context.get()); break;
    default: result = CURLE_UNKNOWN_OPTION; break;
    }
    CURL_ARGS_END();
    return result;
}

CURLcode APS5_VABI curl_easy_getinfo_nid_postfix(CURL* handle, CURLINFO info, ...) {
    if (!handle) return CURLE_UNKNOWN_OPTION;
    if (info == CURLINFO_ACTIVESOCKET || info == CURLINFO_LASTSOCKET) NotImplemented_nid_no_patch(__func__);
    CURL_ARGS_START(info);
    void* output = CURL_ARG(void*);
    CURL_ARGS_END();
    if ((info & CURLINFO_TYPEMASK) == CURLINFO_LONG) {
        long value = 0;
        auto result = ::curl_easy_getinfo(handle, info, output ? &value : nullptr);
        if (result == CURLE_OK) *static_cast<int64_t*>(output) = value;
        return result;
    }
    return ::curl_easy_getinfo(handle, info, output);
}

CURLcode APS5_VABI curl_mime_data_nid_postfix(curl_mimepart* part, const char* data, size_t datasize) {
    return ::curl_mime_data(part, data, datasize);
}

CURLMcode APS5_VABI curl_multi_poll_nid_postfix(CURLM* multi_handle, struct guest_curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms, int* ret) {
    if (extra_nfds) NotImplemented_nid_no_patch(__func__);
    return ::curl_multi_poll(multi_handle, nullptr, 0, timeout_ms, ret);
}

void APS5_VABI curl_easy_cleanup_nid_postfix(CURL* handle) {
    auto context = FindEasy(handle);
    ::curl_easy_cleanup(handle);
    std::lock_guard lock(contextMutex);
    easies.erase(handle);
}

CURLcode APS5_VABI curl_mime_name_nid_postfix(curl_mimepart* part, const char* name) {
    return ::curl_mime_name(part, name);
}

CURLcode APS5_VABI curl_global_init_nid_postfix(long long flags) {
    std::lock_guard lock(globalMutex);
    return GlobalInitWithGuestAllocator(HostLong(flags));
}

CURL* APS5_VABI curl_easy_duphandle_nid_postfix(CURL* handle) {
    auto source = FindEasy(handle);
    auto* easy = ::curl_easy_duphandle(handle);
    if (!easy) return nullptr;
    std::shared_ptr<EasyContext> context;
    try {
        context = std::make_shared<EasyContext>(*source);
        std::lock_guard lock(contextMutex);
        easies.emplace(easy, context);
    } catch (const std::bad_alloc&) {
        ::curl_easy_cleanup(easy);
        return nullptr;
    }
    RebindEasy(easy, context.get());
    return easy;
}

CURLSH* APS5_VABI curl_share_init_nid_postfix() {
    auto* share = ::curl_share_init();
    if (!share) return nullptr;
    try {
        auto context = std::make_shared<ShareContext>();
        ::curl_share_setopt(share, CURLSHOPT_USERDATA, context.get());
        std::lock_guard lock(shareMutex);
        shares.insert_or_assign(share, std::move(context));
    } catch (const std::bad_alloc&) {
        ::curl_share_cleanup(share);
        return nullptr;
    }
    return share;
}

char* APS5_VABI curl_getenv_nid_postfix(const char* variable) {
    return ::curl_getenv(variable);
}

CURLcode APS5_VABI curl_mime_type_nid_postfix(curl_mimepart* part, const char* mimetype) {
    return ::curl_mime_type(part, mimetype);
}

struct guest_curl_slist* APS5_VABI curl_slist_append_nid_postfix(struct guest_curl_slist* list, const char* string) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI curl_easy_reset_nid_postfix(CURL* handle) {
    auto context = FindEasy(handle);
    ::curl_easy_reset(handle);
    *context = EasyContext{};
    BindEasy(handle, context.get());
}

CURL* APS5_VABI curl_easy_init_nid_postfix() {
    {
        std::lock_guard lock(globalMutex);
        if (!globalInitCount && GlobalInitWithGuestAllocator(CURL_GLOBAL_DEFAULT) != CURLE_OK) return nullptr;
    }
    auto* easy = ::curl_easy_init();
    if (!easy) return nullptr;
    std::shared_ptr<EasyContext> context;
    try {
        context = std::make_shared<EasyContext>();
        std::lock_guard lock(contextMutex);
        easies.emplace(easy, context);
    } catch (const std::bad_alloc&) {
        ::curl_easy_cleanup(easy);
        return nullptr;
    }
    BindEasy(easy, context.get());
    return easy;
}

CURLcode APS5_VABI curl_mime_data_cb_nid_postfix(curl_mimepart* part, curl_off_t datasize, guest_curl_read_callback readfunc, guest_curl_seek_callback seekfunc, guest_curl_free_callback freefunc, void* arg) {
    NotImplemented_nid_no_patch(__func__);
}

CURLMcode APS5_VABI curl_multi_fdset_nid_postfix(CURLM* multi_handle, guest_fd_set* read_fd_set, guest_fd_set* write_fd_set, guest_fd_set* exc_fd_set, int* max_fd) {
    NotImplemented_nid_no_patch(__func__);
}

CURLMcode APS5_VABI curl_multi_wakeup_nid_postfix(CURLM* multi_handle) {
    return ::curl_multi_wakeup(multi_handle);
}

CURLMcode APS5_VABI curl_multi_wait_nid_postfix(CURLM* multi_handle, struct guest_curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms, int* ret) {
    if (extra_nfds) NotImplemented_nid_no_patch(__func__);
    return ::curl_multi_wait(multi_handle, nullptr, 0, timeout_ms, ret);
}

const struct guest_curl_easyoption* APS5_VABI curl_easy_option_next_nid_postfix(const struct guest_curl_easyoption* prev) {
    const auto* opt = ::curl_easy_option_next(reinterpret_cast<const curl_easyoption*>(prev));
    return reinterpret_cast<const guest_curl_easyoption*>(opt);
}

void APS5_VABI curl_global_cleanup_nid_postfix() {
    std::lock_guard lock(globalMutex);
    ::curl_global_cleanup();
    if (globalInitCount) --globalInitCount;
}

CURLSHcode APS5_VABI curl_share_setopt_nid_postfix(CURLSH* share_handle, CURLSHoption option, ...) {
    auto context = FindShare(share_handle);
    if (!context) return ::curl_share_setopt(share_handle, option);
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, option);
#define CURL_SHARE_ARG(type) __builtin_va_arg(args, type)
#else
    std::va_list args;
    va_start(args, option);
#define CURL_SHARE_ARG(type) va_arg(args, type)
#endif
    CURLSHcode result;
    switch (option) {
    case CURLSHOPT_SHARE:
    case CURLSHOPT_UNSHARE:
        result = ::curl_share_setopt(share_handle, option, CURL_SHARE_ARG(int));
        break;
    case CURLSHOPT_LOCKFUNC: {
        auto callback = CURL_SHARE_ARG(guest_curl_lock_callback);
        result = ::curl_share_setopt(share_handle, option, callback ? ShareLock : nullptr);
        if (result == CURLSHE_OK) context->lock = callback;
        break;
    }
    case CURLSHOPT_UNLOCKFUNC: {
        auto callback = CURL_SHARE_ARG(guest_curl_unlock_callback);
        result = ::curl_share_setopt(share_handle, option, callback ? ShareUnlock : nullptr);
        if (result == CURLSHE_OK) context->unlock = callback;
        break;
    }
    case CURLSHOPT_USERDATA: {
        auto* data = CURL_SHARE_ARG(void*);
        result = ::curl_share_setopt(share_handle, option, context.get());
        if (result == CURLSHE_OK) context->data = data;
        break;
    }
    default:
        result = ::curl_share_setopt(share_handle, option);
        break;
    }
#undef CURL_SHARE_ARG
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

struct guest_CURLMsg* APS5_VABI curl_multi_info_read_nid_postfix(CURLM* multi_handle, int* msgs_in_queue) {
    return reinterpret_cast<guest_CURLMsg*>(::curl_multi_info_read(multi_handle, msgs_in_queue));
}

CURLcode APS5_VABI curl_easy_upkeep_nid_postfix(CURL* handle) {
    return ::curl_easy_upkeep(handle);
}

CURLMcode APS5_VABI curl_multi_setopt_nid_postfix(CURLM* multi_handle, CURLMoption option, ...) {
    std::shared_ptr<MultiContext> context;
    {
        std::lock_guard lock(contextMutex);
        auto found = multis.find(multi_handle);
        if (found != multis.end()) context = found->second;
    }
    if (!context) return CURLM_BAD_HANDLE;
    if (option == CURLMOPT_SOCKETFUNCTION || option == CURLMOPT_SOCKETDATA ||
        option == CURLMOPT_PUSHFUNCTION || option == CURLMOPT_PUSHDATA) NotImplemented_nid_no_patch(__func__);
    switch (option) {
    case CURLMOPT_TIMERFUNCTION:
    case CURLMOPT_TIMERDATA:
    case CURLMOPT_PIPELINING:
    case CURLMOPT_MAXCONNECTS:
    case CURLMOPT_MAX_HOST_CONNECTIONS:
    case CURLMOPT_MAX_PIPELINE_LENGTH:
    case CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE:
    case CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE:
    case CURLMOPT_PIPELINING_SITE_BL:
    case CURLMOPT_PIPELINING_SERVER_BL:
    case CURLMOPT_MAX_TOTAL_CONNECTIONS:
    case CURLMOPT_MAX_CONCURRENT_STREAMS:
        break;
    default:
        return ::curl_multi_setopt(multi_handle, option, nullptr);
    }
    CURL_ARGS_START(option);
    CURLMcode result;
    if (option == CURLMOPT_TIMERFUNCTION) {
        auto* value = CURL_ARG(void*);
        result = ::curl_multi_setopt(multi_handle, CURLMOPT_TIMERDATA, context.get());
        if (result == CURLM_OK) result = ::curl_multi_setopt(multi_handle, option, value ? Timer : nullptr);
        if (result == CURLM_OK) context->timer = value;
    } else if (option == CURLMOPT_TIMERDATA) {
        auto* value = CURL_ARG(void*);
        result = ::curl_multi_setopt(multi_handle, option, context.get());
        if (result == CURLM_OK) context->timerData = value;
    } else if (static_cast<int>(option) / 10000 == 0) {
        auto value = CURL_ARG(int64_t);
        CURL_ARGS_END();
        return ::curl_multi_setopt(multi_handle, option, HostLong(value));
    } else if (static_cast<int>(option) / 10000 == 3) {
        result = ::curl_multi_setopt(multi_handle, option, CURL_ARG(curl_off_t));
    } else if (static_cast<int>(option) / 10000 == 1) {
        result = ::curl_multi_setopt(multi_handle, option, CURL_ARG(void*));
    } else result = ::curl_multi_setopt(multi_handle, option, nullptr);
    CURL_ARGS_END();
    return result;
}

curl_mimepart* APS5_VABI curl_mime_addpart_nid_postfix(curl_mime* mime) {
    return ::curl_mime_addpart(mime);
}

const struct guest_curl_easyoption* APS5_VABI curl_easy_option_by_name_nid_postfix(const char* name) {
    const auto* opt = ::curl_easy_option_by_name(name);
    return reinterpret_cast<const guest_curl_easyoption*>(opt);
}

void APS5_VABI curl_slist_free_all_nid_postfix(struct guest_curl_slist* list) {
    NotImplemented_nid_no_patch(__func__);
}

std::time_t APS5_VABI curl_getdate_nid_postfix(const char* p, const std::time_t* unused) {
    return ::curl_getdate(p, unused);
}

CURLcode APS5_VABI curl_mime_filedata_nid_postfix(curl_mimepart* part, const char* filename) {
    return ::curl_mime_filedata(part, filename);
}

CURLMcode APS5_VABI curl_multi_timeout_nid_postfix(CURLM* multi_handle, int64_t* milliseconds) {
    long host_ms = 0;
    const CURLMcode res = ::curl_multi_timeout(multi_handle, milliseconds ? &host_ms : nullptr);
    if (res == CURLM_OK) {
        *milliseconds = host_ms;
    }
    return res;
}

CURLMcode APS5_VABI curl_multi_add_handle_nid_postfix(CURLM* multi_handle, CURL* easy_handle) {
    return ::curl_multi_add_handle(multi_handle, easy_handle);
}

curl_mime* APS5_VABI curl_mime_init_nid_postfix(CURL* easy) {
    return ::curl_mime_init(easy);
}

const struct guest_curl_easyoption* APS5_VABI curl_easy_option_by_id_nid_postfix(CURLoption id) {
    const auto* opt = ::curl_easy_option_by_id(id);
    return reinterpret_cast<const guest_curl_easyoption*>(opt);
}

CURLcode APS5_VABI curl_mime_subparts_nid_postfix(curl_mimepart* part, curl_mime* subparts) {
    return ::curl_mime_subparts(part, subparts);
}

struct guest_curl_version_info_data* APS5_VABI curl_version_info_nid_postfix(CURLversion age) {
    static guest_curl_version_info_data guest_info{};
    const auto* info = ::curl_version_info(age);
    if (!info) return nullptr;
    guest_info.age = info->age;
    guest_info.version = info->version;
    guest_info.version_num = info->version_num;
    guest_info.host = info->host;
    guest_info.features = info->features;
    guest_info.ssl_version = info->ssl_version;
    guest_info.ssl_version_num = info->ssl_version_num;
    guest_info.libz_version = info->libz_version;
    guest_info.protocols = info->protocols;
    guest_info.ares = info->ares;
    guest_info.ares_num = info->ares_num;
    guest_info.libidn = info->libidn;
    guest_info.iconv_ver_num = info->iconv_ver_num;
    guest_info.libssh_version = info->libssh_version;
    guest_info.brotli_ver_num = info->brotli_ver_num;
    guest_info.brotli_version = info->brotli_version;
    guest_info.nghttp2_ver_num = info->nghttp2_ver_num;
    guest_info.nghttp2_version = info->nghttp2_version;
    guest_info.quic_version = info->quic_version;
    guest_info.cainfo = info->cainfo;
    guest_info.capath = info->capath;
    guest_info.zstd_ver_num = info->zstd_ver_num;
    guest_info.zstd_version = info->zstd_version;
    guest_info.hyper_version = info->hyper_version;
    guest_info.gsasl_version = info->gsasl_version;
    return &guest_info;
}

CURLcode APS5_VABI curl_mime_encoder_nid_postfix(curl_mimepart* part, const char* encoding) {
    return ::curl_mime_encoder(part, encoding);
}

CURLcode APS5_VABI curl_global_init_mem_nid_postfix(long long flags, guest_curl_malloc_callback m, guest_curl_free_callback f, guest_curl_realloc_callback r, guest_curl_strdup_callback s, guest_curl_calloc_callback c) {
    NotImplemented_nid_no_patch(__func__);
}

CURLcode APS5_VABI curl_mime_headers_nid_postfix(curl_mimepart* part, struct guest_curl_slist* headers, int take_ownership) {
    return ::curl_mime_headers(part, reinterpret_cast<curl_slist*>(headers), take_ownership);
}

CURLM* APS5_VABI curl_multi_init_nid_postfix() {
    auto* multi = ::curl_multi_init();
    if (!multi) return nullptr;
    try {
        auto context = std::make_shared<MultiContext>();
        std::lock_guard lock(contextMutex);
        multis.emplace(multi, std::move(context));
    } catch (const std::bad_alloc&) {
        ::curl_multi_cleanup(multi);
        return nullptr;
    }
    return multi;
}

CURLMcode APS5_VABI curl_multi_cleanup_nid_postfix(CURLM* multi_handle) {
    auto result = ::curl_multi_cleanup(multi_handle);
    if (result == CURLM_OK) {
        std::lock_guard lock(contextMutex);
        multis.erase(multi_handle);
    }
    return result;
}

CURLcode APS5_VABI curl_easy_pause_nid_postfix(CURL* handle, int bitmask) {
    return ::curl_easy_pause(handle, bitmask);
}

CURLcode APS5_VABI curl_mime_filename_nid_postfix(curl_mimepart* part, const char* filename) {
    return ::curl_mime_filename(part, filename);
}

}

#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using SyncOperation = int (APS5_VABI*)(void*);

static_assert(std::is_same_v<decltype(&_ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix), SyncOperation>);

struct FilebufStorage {
    std::array<std::byte, 0xa8> prefix;
    FileStream* file;
    std::array<std::byte, 32> suffix;
};

static_assert(offsetof(FilebufStorage, file) == 0xa8);
static_assert(offsetof(GuestLocale::StreambufVtable, overflow) == 0x20);

struct FakeVtable {
    std::array<std::uintptr_t, 5> slots;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "filebuf io: %s\n", message);
    std::abort();
}

template <typename TCall>
void RequireNotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("not implemented") != std::string::npos,
            "filebuf sync threw an unexpected runtime error");
        return;
    }
    Require(false, "filebuf sync returned instead of throwing NotImplemented");
}

unsigned overflowCalls = 0;
void* overflowSelf = nullptr;
int overflowArgument = 0;
int overflowResult = 0;
FileStream* overflowStream = nullptr;
const char* overflowPayload = nullptr;
std::size_t overflowPayloadSize = 0;

int APS5_VABI OverflowProbe(GuestLocale::Streambuf* self, int character) {
    ++overflowCalls;
    overflowSelf = self;
    overflowArgument = character;
    if (overflowStream != nullptr && overflowPayload != nullptr && overflowPayloadSize != 0) {
        Require(fwrite_nid_postfix(overflowPayload, 1, overflowPayloadSize, overflowStream) == overflowPayloadSize,
            "overflow wrote buffered payload to guest stream");
    }
    return overflowResult;
}

FilebufStorage MakeObject(FileStream* file, const void* vtable) {
    FilebufStorage object{};
    object.prefix.fill(std::byte{0xa5});
    object.suffix.fill(std::byte{0x5a});
    object.file = file;
    std::memcpy(object.prefix.data(), &vtable, sizeof(vtable));
    return object;
}

void ResetOverflowState(int result, FileStream* stream, const char* payload, std::size_t payloadSize) {
    overflowCalls = 0;
    overflowSelf = nullptr;
    overflowArgument = 0;
    overflowResult = result;
    overflowStream = stream;
    overflowPayload = payload;
    overflowPayloadSize = payloadSize;
}

void ReplaceWithReadOnlyDescriptor(FileStream& stream) {
#ifdef _WIN32
    const int descriptor = ::_open("NUL", _O_RDONLY | _O_BINARY);
    Require(descriptor >= 0 && ::_dup2(descriptor, ::_fileno(stream.GetHandle())) == 0,
        "stream descriptor replaced with read-only descriptor");
    Require(::_close(descriptor) == 0, "spare descriptor closed");
#else
    const int descriptor = ::open("/dev/null", O_RDONLY);
    Require(descriptor >= 0 && ::dup2(descriptor, ::fileno(stream.GetHandle())) >= 0,
        "stream descriptor replaced with read-only descriptor");
    Require(::close(descriptor) == 0, "spare descriptor closed");
#endif
}

long ReadDescriptorFromStart(FileStream& stream, char* buffer, std::size_t bytes) {
#ifdef _WIN32
    const int fd = ::_fileno(stream.GetHandle());
    if (::_lseeki64(fd, 0, SEEK_SET) < 0) return -1;
    return ::_read(fd, buffer, static_cast<unsigned>(bytes));
#else
    return static_cast<long>(::pread(::fileno(stream.GetHandle()), buffer, bytes, 0));
#endif
}

void CheckSyncClosedFile() {
    FakeVtable vtable{{0, 0, 0, 0, reinterpret_cast<std::uintptr_t>(&OverflowProbe)}};
    FilebufStorage object = MakeObject(nullptr, vtable.slots.data());
    const auto original = object;
    ResetOverflowState(0, nullptr, nullptr, 0);
    errno = 71;
    Require(_ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&object) == 0,
        "closed file sync returns 0");
    Require(errno == 71, "closed file sync preserves errno");
    Require(overflowCalls == 0, "closed file sync does not invoke overflow");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0,
        "closed file sync leaves object bytes unchanged");
}

void CheckSyncFlushesWrittenBytes() {
    FileStream stream(TemporaryFile::Open());
    FakeVtable vtable{{0, 0, 0, 0, reinterpret_cast<std::uintptr_t>(&OverflowProbe)}};
    auto object = MakeObject(&stream, vtable.slots.data());
    const auto original = object;
    constexpr char kPayload[] = "sync-payload";
    constexpr std::size_t kPayloadLength = sizeof(kPayload) - 1;
    ResetOverflowState(0, &stream, kPayload, kPayloadLength);

    errno = 88;
    const int result = _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&object);
    Require(result == 0, "sync on open stream returns 0");
    Require(errno == 88, "sync on open stream preserves errno");
    Require(overflowCalls == 1, "sync invokes overflow once");
    Require(overflowSelf == &object, "sync passes exact self pointer to overflow");
    Require(overflowArgument == -1, "sync passes eof (-1) to overflow");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0,
        "sync leaves filebuf object bytes unchanged");

    std::array<char, kPayloadLength + 4> diskBytes{};
    const long readCount = ReadDescriptorFromStart(stream, diskBytes.data(), diskBytes.size());
    Require(readCount == static_cast<long>(kPayloadLength),
        "sync flushed exact payload byte count to descriptor");
    Require(std::memcmp(diskBytes.data(), kPayload, kPayloadLength) == 0,
        "sync flushed exact payload bytes to descriptor");
}

void CheckSyncFailingOverflow() {
    FileStream stream(TemporaryFile::Open());
    Require(fwrite_nid_postfix("keep", 1, 4, &stream) == 4, "staged buffered bytes");
    FakeVtable vtable{{0, 0, 0, 0, reinterpret_cast<std::uintptr_t>(&OverflowProbe)}};
    auto object = MakeObject(&stream, vtable.slots.data());
    const auto original = object;
    ResetOverflowState(-1, nullptr, nullptr, 0);

    errno = 77;
    const int result = _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&object);
    Require(result == 0, "failing overflow returns 0 from sync");
    Require(errno == 77, "failing overflow preserves errno");
    Require(overflowCalls == 1, "failing overflow was invoked once");
    Require(overflowSelf == &object && overflowArgument == -1,
        "failing overflow received self and -1");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0,
        "failing overflow leaves object bytes unchanged");

    std::array<char, 8> diskBytes{};
    const long beforeFlushRead = ReadDescriptorFromStart(stream, diskBytes.data(), diskBytes.size());
    Require(beforeFlushRead == 0, "failing overflow skipped fflush on buffered stream");
}

void CheckSyncFailingFlush() {
    FileStream stream(TemporaryFile::Open());
    Require(std::fputc('X', stream.GetHandle()) == 'X', "dirty write staged in stream buffer");
    ReplaceWithReadOnlyDescriptor(stream);
    FakeVtable vtable{{0, 0, 0, 0, reinterpret_cast<std::uintptr_t>(&OverflowProbe)}};
    auto object = MakeObject(&stream, vtable.slots.data());
    ResetOverflowState(0, nullptr, nullptr, 0);

    const int result = _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&object);
    Require(result == -1, "failing fflush returns -1 from sync");
    Require(overflowCalls == 1, "failing fflush still invoked overflow first");
    Require(overflowSelf == &object && overflowArgument == -1,
        "failing fflush overflow call received self and -1");
}

void CheckSyncNullVtableAndOverflow() {
    FileStream stream(TemporaryFile::Open());
    auto nullVtableObject = MakeObject(&stream, nullptr);
    RequireNotImplemented([&] {
        _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&nullVtableObject);
    });

    FakeVtable nullOverflowVtable{};
    auto nullOverflowObject = MakeObject(&stream, nullOverflowVtable.slots.data());
    RequireNotImplemented([&] {
        _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(&nullOverflowObject);
    });
}

}

int main() {
    CheckSyncClosedFile();
    CheckSyncFlushesWrittenBytes();
    CheckSyncFailingOverflow();
    CheckSyncFailingFlush();
    CheckSyncNullVtableAndOverflow();
    std::puts("filebuf io: sync checks passed");
    return 0;
}

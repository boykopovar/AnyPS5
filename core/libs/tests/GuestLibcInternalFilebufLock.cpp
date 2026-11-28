#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <type_traits>

namespace {

using Operation = void (APS5_VABI*)(void*);

static_assert(std::is_same_v<decltype(&_ZNSt13basic_filebufIcSt11char_traitsIcEE5_LockEv_nid_postfix), Operation>);
static_assert(std::is_same_v<decltype(&_ZNSt13basic_filebufIwSt11char_traitsIwEE5_LockEv_nid_postfix), Operation>);

struct FilebufStorage {
    std::array<std::byte, 0xa8> prefix;
    FileStream* file;
    std::array<std::byte, 32> suffix;
};

static_assert(offsetof(FilebufStorage, file) == 0xa8);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "filebuf lock: %s\n", message);
    std::abort();
}

int TryFromOtherThread(FileStream* stream) {
    int result = 0;
    std::thread thread([&] {
        result = ftrylockfile_nid_postfix(stream);
        if (result == 0) funlockfile_nid_postfix(stream);
    });
    thread.join();
    return result;
}

void Check(Operation lock, Operation unlock) {
    FilebufStorage object{};
    object.prefix.fill(std::byte{0xa5});
    object.suffix.fill(std::byte{0x5a});
    const auto empty = object;
    errno = 71;
    lock(&object);
    Require(errno == 71, "an empty file buffer preserves errno");
    Require(std::memcmp(&object, &empty, sizeof(object)) == 0, "an empty file buffer is unchanged");

    FileStream stream(TemporaryFile::Open());
    FileStream neighbor(TemporaryFile::Open());
    FileStream* adjacent = &neighbor;
    std::memcpy(object.prefix.data() + object.prefix.size() - sizeof(adjacent), &adjacent, sizeof(adjacent));
    std::memcpy(object.suffix.data(), &adjacent, sizeof(adjacent));
    object.file = &stream;
    const auto original = object;
    Require(TryFromOtherThread(&stream) == 0, "the initial stream lock is free");

    lock(&object);
    Require(TryFromOtherThread(&stream) == -1, "the first call excludes another thread");
    Require(TryFromOtherThread(&neighbor) == 0, "the neighboring stream is not locked");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0, "locking leaves the file buffer unchanged");
    lock(&object);
    Require(TryFromOtherThread(&stream) == -1, "recursive acquisition retains ownership");
    unlock(&object);
    Require(TryFromOtherThread(&stream) == -1, "one release retains the outer lock");
    unlock(&object);
    Require(TryFromOtherThread(&stream) == 0, "two releases restore access to another thread");
    Require(TryFromOtherThread(&neighbor) == 0, "the neighboring stream remains free");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0, "the complete lock cycle preserves the object");
}

}

int main() {
    Check(_ZNSt13basic_filebufIcSt11char_traitsIcEE5_LockEv_nid_postfix,
        _ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv_nid_postfix);
    Check(_ZNSt13basic_filebufIwSt11char_traitsIwEE5_LockEv_nid_postfix,
        _ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv_nid_postfix);
    std::puts("filebuf lock: narrow and wide checks passed");
}

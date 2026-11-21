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

using Unlock = void (APS5_VABI*)(void*);

static_assert(std::is_same_v<decltype(&_ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv_nid_postfix), Unlock>);
static_assert(std::is_same_v<decltype(&_ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv_nid_postfix), Unlock>);

struct FilebufStorage {
    std::array<std::byte, 0xa8> prefix;
    FileStream* file;
    std::array<std::byte, 32> suffix;
};

static_assert(offsetof(FilebufStorage, file) == 0xa8);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "filebuf unlock: %s\n", message);
    std::abort();
}

int TryFromOtherThread(FileStream* stream) {
    int result = 0;
    std::thread other([&] {
        result = ftrylockfile_nid_postfix(stream);
        if (result == 0) funlockfile_nid_postfix(stream);
    });
    other.join();
    return result;
}

void Check(Unlock unlock) {
    FilebufStorage object{};
    object.prefix.fill(std::byte{0xa5});
    object.suffix.fill(std::byte{0x5a});
    const auto empty = object;
    errno = 71;
    unlock(&object);
    Require(errno == 71, "an empty file buffer preserves errno");
    Require(std::memcmp(&object, &empty, sizeof(object)) == 0, "an empty file buffer is unchanged");

    FileStream stream(TemporaryFile::Open());
    FileStream neighbor(TemporaryFile::Open());
    std::memcpy(object.prefix.data() + object.prefix.size() - sizeof(FileStream*), &empty.file, sizeof(FileStream*));
    FileStream* adjacent = &neighbor;
    std::memcpy(object.suffix.data(), &adjacent, sizeof(adjacent));
    object.file = &stream;
    const auto original = object;

    flockfile_nid_postfix(&stream);
    flockfile_nid_postfix(&stream);
    flockfile_nid_postfix(&neighbor);
    Require(TryFromOtherThread(&stream) == -1, "another thread cannot enter the recursive lock");
    unlock(&object);
    Require(TryFromOtherThread(&stream) == -1, "one unlock retains the outer recursive lock");
    Require(TryFromOtherThread(&neighbor) == -1, "the neighboring stream remains locked");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0, "one unlock leaves the file buffer unchanged");

    unlock(&object);
    Require(TryFromOtherThread(&stream) == 0, "the second unlock releases the stream to another thread");
    Require(TryFromOtherThread(&neighbor) == -1, "the second unlock does not release the neighboring stream");
    Require(std::memcmp(&object, &original, sizeof(object)) == 0, "the final unlock leaves the file buffer unchanged");
    funlockfile_nid_postfix(&neighbor);
}

}

int main() {
    Check(_ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv_nid_postfix);
    Check(_ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv_nid_postfix);
    std::puts("filebuf unlock: narrow and wide checks passed");
}

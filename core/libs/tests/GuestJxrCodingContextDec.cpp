#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
std::size_t JxrContextDecCodecBytes_nid_no_patch();
void* JxrContextDecMake_nid_no_patch(std::uint32_t seed, std::int32_t format);
void* JxrContextDecClone_nid_no_patch(const void* source);
void JxrContextDecRelease_nid_no_patch(void* codec);
std::int32_t JxrContextDecReference_nid_no_patch(void* codec, std::int32_t count);
std::size_t JxrContextDecCount_nid_no_patch(const void* codec);
std::int32_t JxrContextDecEqual_nid_no_patch(const void* guest, const void* native);
}

namespace {

constexpr std::int32_t ColorFormats = 9;
constexpr std::int32_t MaxContexts = 4096;
constexpr std::int32_t Error = -1;

struct HostWriteRecord {
    std::uintptr_t address;
    std::size_t bytes;
};

std::vector<HostWriteRecord> hostWrites;

void RecordHostWrite(std::uintptr_t address, std::size_t bytes) {
    hostWrites.push_back({address, bytes});
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR AllocateCodingContextDec: %s\n", message);
    std::abort();
}

struct Pair {
    void* guest;
    void* native;
    Pair(std::uint32_t seed, std::int32_t format) {
        guest = JxrContextDecMake_nid_no_patch(seed, format);
        Require(guest != nullptr, "allocation");
        native = JxrContextDecClone_nid_no_patch(guest);
        Require(native != nullptr, "allocation");
    }
    ~Pair() {
        JxrContextDecRelease_nid_no_patch(guest);
        JxrContextDecRelease_nid_no_patch(native);
    }
    Pair(const Pair&) = delete;
    Pair& operator=(const Pair&) = delete;
};

void TestAllocation() {
    std::uint32_t seed = 1;
    for (std::int32_t format = 0; format < ColorFormats; ++format) {
        for (const std::int32_t count : {1, 2, 3, 17, MaxContexts}) {
            Pair pair(seed++, format);
            hostWrites.clear();
            Require(AllocateCodingContextDec_nid_postfix(pair.guest, count) == 0, "valid count fails");
            Require(hostWrites.size() == 1 && hostWrites[0].address == reinterpret_cast<std::uintptr_t>(pair.guest) &&
                        hostWrites[0].bytes == JxrContextDecCodecBytes_nid_no_patch(),
                    "codec write is not opened");
            Require(JxrContextDecReference_nid_no_patch(pair.native, count) == 0, "jxrlib rejects a valid count");
            Require(JxrContextDecCount_nid_no_patch(pair.guest) == static_cast<std::size_t>(count), "context count");
            Require(JxrContextDecEqual_nid_no_patch(pair.guest, pair.native) != 0, "contexts differ from jxrlib");
        }
    }
}

void TestInvalidCounts() {
    std::uint32_t seed = 100;
    for (std::int32_t format = 0; format < ColorFormats; ++format) {
        for (const std::int32_t count : {0, -1, -MaxContexts, MaxContexts + 1, INT_MIN, INT_MAX}) {
            Pair pair(seed++, format);
            const std::vector<unsigned char> before(static_cast<unsigned char*>(pair.guest),
                static_cast<unsigned char*>(pair.guest) + JxrContextDecCodecBytes_nid_no_patch());
            hostWrites.clear();
            Require(AllocateCodingContextDec_nid_postfix(pair.guest, count) == Error, "invalid count accepted");
            Require(hostWrites.empty(), "invalid count opens a codec write");
            Require(JxrContextDecReference_nid_no_patch(pair.native, count) == Error, "jxrlib accepts an invalid count");
            Require(std::memcmp(pair.guest, before.data(), before.size()) == 0, "invalid count changes the codec");
            Require(JxrContextDecEqual_nid_no_patch(pair.guest, pair.native) != 0, "invalid count differs from jxrlib");
        }
    }
}

void TestNullCodec() {
    for (const std::int32_t count : {1, MaxContexts, 0, MaxContexts + 1, INT_MIN}) {
        hostWrites.clear();
        Require(AllocateCodingContextDec_nid_postfix(nullptr, count) == Error, "null codec accepted");
        Require(hostWrites.empty(), "null codec opens a write");
        Require(JxrContextDecReference_nid_no_patch(nullptr, count) == Error, "jxrlib accepts a null codec");
    }
}

} // namespace

int main() {
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(RecordHostWrite);
    TestAllocation();
    TestInvalidCounts();
    TestNullCodec();
    std::puts("JXR AllocateCodingContextDec checks passed");
    return 0;
}

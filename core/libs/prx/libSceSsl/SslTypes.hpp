#pragma once

#include <cstddef>
#include <cstdint>

namespace SceSsl {

struct Data {
    std::uint8_t* ptr;
    std::size_t size;
};

struct CaCerts {
    Data* certs;
    std::size_t num;
    void* pool;
};

struct CaList {
    void** certs;
    int num;
};

struct MemoryPoolStats {
    std::size_t poolSize, maxInuseSize, currentInuseSize;
    std::uint32_t reserved;
};

struct RtcTick {
    std::uint64_t tick;
};

static_assert(sizeof(Data) == 0x10);
static_assert(sizeof(CaCerts) == 0x18);
static_assert(sizeof(CaList) == 0x10);
static_assert(sizeof(MemoryPoolStats) == 0x20);
static_assert(sizeof(RtcTick) == 0x8);

}

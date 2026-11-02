#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_SLABRETURN_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_SLABRETURN_HPP

#include "AllocationFailure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>

namespace AgcDriver::Tests {

inline void SlabReturnWithoutAllocation(const Graphics::Context& context, std::uint32_t memoryType) {
    constexpr std::size_t capacity = std::size_t{512} << 10u;
    constexpr std::size_t slotsPerBlock = 8;
    Graphics::Require(Graphics::BufferPool::SlabBlockBytes(capacity) == capacity * slotsPerBlock, "unexpected slab size in the return test");
    Graphics::BufferPool pool(context);
    std::array<Graphics::SlabSlot, 6 * slotsPerBlock> slots{};
    std::array<Graphics::SlabSlot, 6> reused{};
    unsigned returned = 0;
    for (const std::size_t blocks : {4u, 6u, 3u}) {
        std::printf("slab return type=%u blocks=%zu filling\n", memoryType, blocks);
        std::fflush(stdout);
        const auto count = blocks * slotsPerBlock;
        for (std::size_t index = 0; index < count; ++index) {
            slots[index] = pool.TakeSlot(context, memoryType, capacity, false);
            if (slots[index].mapping != nullptr) std::memcpy(slots[index].mapping, &index, sizeof(index));
        }
        Graphics::Require(pool.SlabBlocks() == blocks, "the return test did not fill every slab");
        std::puts("slab return arming host allocation failure");
        std::fflush(stdout);
        const auto returnSlots = [&] {
            const auto previous = std::set_terminate([] {
                std::fputs("FAIL: slab return terminated during host allocation failure\n", stderr);
                std::fflush(stderr);
                std::_Exit(1);
            });
            SetAllocationFailureCountdown(0);
            for (std::size_t block = 0; block < blocks; ++block) pool.PutSlot(reused[block].memory, reused[block].offset);
            SetAllocationFailureCountdown(-1);
            std::set_terminate(previous);
        };
        for (std::size_t block = 0; block < blocks; ++block) reused[block] = slots[block * slotsPerBlock];
        returnSlots();
        for (std::size_t block = 0; block < blocks; ++block) {
            const auto index = block * slotsPerBlock + 1;
            if (slots[index].mapping == nullptr) continue;
            std::size_t value = 0;
            std::memcpy(&value, slots[index].mapping, sizeof(value));
            Graphics::Require(value == index, "returning a slot changed a live sibling");
        }
        std::array<bool, 6> seen{};
        for (std::size_t block = 0; block < blocks; ++block) {
            reused[block] = pool.TakeSlot(context, memoryType, capacity, false);
            bool matched = false;
            for (std::size_t candidate = 0; candidate < blocks; ++candidate) {
                const auto& expected = slots[candidate * slotsPerBlock];
                if (reused[block].memory != expected.memory || reused[block].offset != expected.offset) continue;
                Graphics::Require(!seen[candidate], "a returned slot was duplicated");
                seen[candidate] = true;
                matched = true;
            }
            Graphics::Require(matched, "a returned slot was lost");
        }
        returnSlots();
        VkDeviceSize released = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (index % slotsPerBlock != 0) released += pool.PutSlot(slots[index].memory, slots[index].offset);
        }
        Graphics::Require(pool.SlabBlocks() == 1 && released == (blocks - 1) * capacity * slotsPerBlock, "empty slab retirement retained the wrong number of blocks");
        returned += static_cast<unsigned>(blocks * 2);
    }
    std::printf("slab return type=%u cycles=3 returned=%u host allocation failures armed\n", memoryType, returned);
}

}

#endif

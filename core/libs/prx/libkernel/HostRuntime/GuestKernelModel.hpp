#ifndef ANYPS5_GUEST_KERNEL_MODEL_HPP
#define ANYPS5_GUEST_KERNEL_MODEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

// A bounded, deterministic model used by a future guest syscall trap. Guest
// addresses are never dereferenced as host pointers. This models memory and
// pipe mechanics, not PS5 firmware structures or privileged kernel state.
class GuestKernelModel {
public:
    explicit GuestKernelModel(int pid = 1000) : pid_(pid) {}

    void MapUser(std::uint64_t address, std::size_t size);
    void MapKernel(std::uint64_t address, std::size_t size);
    bool ReadUser(std::uint64_t address, std::span<std::byte> output) const;
    bool WriteUser(std::uint64_t address, std::span<const std::byte> input);
    bool CopyKernelOut(std::uint64_t address, std::span<std::byte> output) const;
    bool CopyKernelIn(std::uint64_t address, std::span<const std::byte> input);
    std::array<int, 2> CreatePipe();

    // FreeBSD/PS5 syscall numbers and negative FreeBSD errno on failure.
    // 3=read, 4=write, 6=close, 20=getpid, 542=pipe2. Unknown calls fail
    // explicitly with ENOSYS; no host syscall number is forwarded.
    std::int64_t Dispatch(std::uint64_t number, const std::array<std::uint64_t, 6>& args);

private:
    struct Region { std::uint64_t address; std::vector<std::byte> bytes; };
    struct Pipe { int readFd; int writeFd; bool readOpen = true; bool writeOpen = true; std::deque<std::byte> bytes; };
    static std::span<std::byte> Locate(std::vector<Region>& regions, std::uint64_t address, std::size_t size);
    static std::span<const std::byte> Locate(const std::vector<Region>& regions, std::uint64_t address, std::size_t size);
    static void Map(std::vector<Region>& regions, std::uint64_t address, std::size_t size);
    std::vector<Region> user_;
    std::vector<Region> kernel_;
    std::vector<Pipe> pipes_;
    int nextFd_ = 16;
    int pid_;
};

#endif

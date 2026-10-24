#include <codegen/x86/RedZoneFramer.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__) && defined(__x86_64__)
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#endif

#if defined(__linux__) && defined(__x86_64__)

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::size_t kArenaBytes = 0x10000;
constexpr std::size_t kStubBase = 0x4000;
constexpr std::size_t kRunnerBase = 0xc000;
constexpr std::uint8_t kClobber = 0xa5;
constexpr std::uint64_t kA = 0x1111, kB = 0x2222, kC = 0x3333, kD = 0x4444;

std::uint8_t* arena = nullptr;
volatile sig_atomic_t steps = 0;

void clobberBelowRsp(int, siginfo_t*, void* context) {
    auto& registers = static_cast<ucontext_t*>(context)->uc_mcontext.gregs;
    const auto rip = static_cast<std::uintptr_t>(registers[REG_RIP]);
    const auto base = reinterpret_cast<std::uintptr_t>(arena);
    if (rip < base || rip >= base + kArenaBytes) {
        registers[REG_EFL] &= ~static_cast<greg_t>(0x100);
        return;
    }
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(registers[REG_RSP]) - 0x80), kClobber, 0x80);
    steps = steps + 1;
}

const std::vector<std::uint8_t> Runner{
    0x53, 0x4c, 0x89, 0xc3, 0x48, 0x89, 0xf8, 0x48, 0x89, 0xf7, 0x48, 0x89, 0xd6, 0x48, 0x89, 0xca,
    0x4d, 0x85, 0xc9, 0x74, 0x0a, 0x9c, 0x48, 0x81, 0x0c, 0x24, 0x00, 0x01, 0x00, 0x00, 0x9d,
    0xff, 0xd0, 0x9c, 0x48, 0x81, 0x24, 0x24, 0xff, 0xfe, 0xff, 0xff, 0x9d, 0x5b, 0xc3,
};

struct Leaf {
    const char* Name;
    std::vector<std::uint8_t> Code;
    std::uint64_t Expected;
};

const std::vector<std::uint8_t> FramelessPushes{0x53, 0x41, 0x56, 0x41, 0x57};
const std::vector<std::uint8_t> FramelessPops{0x41, 0x5f, 0x41, 0x5e, 0x5b, 0xc3};

std::vector<std::uint8_t> frameless(const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> bytes = FramelessPushes;
    bytes.insert(bytes.end(), body.begin(), body.end());
    bytes.insert(bytes.end(), FramelessPops.begin(), FramelessPops.end());
    return bytes;
}

std::vector<Leaf> framedLeaves() {
    return {
        {"frameless leaf spilling 0x10 below rsp", frameless({
            0x48, 0x89, 0x7c, 0x24, 0xf0, 0x48, 0x89, 0x74, 0x24, 0xf8, 0x31, 0xc0, 0xb9, 0x08, 0x00, 0x00, 0x00,
            0x48, 0x03, 0x44, 0x24, 0xf0, 0x48, 0x03, 0x44, 0x24, 0xf8, 0xff, 0xc9, 0x75, 0xf2,
        }), 8 * (kA + kB)},
        {"leaf reaching 0x28 below rsp", frameless({
            0x48, 0x89, 0x7c, 0x24, 0xd8, 0x48, 0x89, 0x74, 0x24, 0xe8, 0x48, 0x89, 0x54, 0x24, 0xf0, 0x31, 0xc0, 0xb9, 0x08, 0x00, 0x00, 0x00,
            0x48, 0x03, 0x44, 0x24, 0xd8, 0x48, 0x03, 0x44, 0x24, 0xe8, 0x48, 0x03, 0x44, 0x24, 0xf0, 0xff, 0xc9, 0x75, 0xed,
        }), 8 * (kA + kB + kC)},
        {"leaf reading its saved rbx at +0x10 moved in place", frameless({
            0x48, 0x89, 0x7c, 0x24, 0xf0, 0x31, 0xc0, 0xb9, 0x08, 0x00, 0x00, 0x00,
            0x48, 0x03, 0x44, 0x24, 0xf0, 0x48, 0x03, 0x44, 0x24, 0x10, 0xff, 0xc9, 0x75, 0xf2,
        }), 8 * (kA + kD)},
        {"leaf taking the address of a slot 0x20 below rsp", frameless({
            0x4c, 0x8d, 0x74, 0x24, 0xe0, 0x49, 0x89, 0x3e, 0x49, 0x89, 0x76, 0x08, 0x31, 0xc0, 0xb9, 0x08, 0x00, 0x00, 0x00,
            0x49, 0x03, 0x06, 0x49, 0x03, 0x46, 0x08, 0xff, 0xc9, 0x75, 0xf5,
        }), 8 * (kA + kB)},
        {"inflate_fast-shaped leaf allocating 0x40 after six pushes", {
            0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x40,
            0x48, 0x89, 0x7c, 0x24, 0xe0, 0x48, 0x89, 0x7c, 0x24, 0x00, 0x48, 0x89, 0x74, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x38,
            0xb9, 0x04, 0x00, 0x00, 0x00, 0x31, 0xc0, 0x48, 0x03, 0x44, 0x24, 0x08, 0x48, 0x03, 0x44, 0x24, 0xe0, 0x48, 0x03, 0x44, 0x24, 0x00,
            0xff, 0xc9, 0x75, 0xed, 0x48, 0x8b, 0x54, 0x24, 0x38,
            0x48, 0x83, 0xc4, 0x40, 0x5b, 0x41, 0x5c, 0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
        }, 4 * (kB + 2 * kA)},
    };
}

std::vector<Leaf> stubbedLeaves() {
    return {
        {"inflate_fast-shaped leaf with operands at rsp and at 0x60 shifted by 0x20", {
            0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x70,
            0x48, 0x89, 0x7c, 0x24, 0xe0, 0x48, 0x89, 0x34, 0x24, 0x48, 0x89, 0x54, 0x24, 0x60, 0xb9, 0x04, 0x00, 0x00, 0x00, 0x31, 0xc0,
            0x48, 0x03, 0x44, 0x24, 0xe0, 0x48, 0x03, 0x04, 0x24, 0x48, 0x03, 0x44, 0x24, 0x60, 0xff, 0xc9, 0x75, 0xee,
            0x48, 0x83, 0xc4, 0x70, 0x5b, 0x41, 0x5c, 0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
        }, 4 * (kA + kB + kC)},
        {"leaf shifted by the whole red zone reading its saved rbx at +0x8", {
            0x41, 0x57, 0x53, 0x41, 0x56, 0x4c, 0x8d, 0x74, 0x24, 0xe0, 0x49, 0x89, 0x3e, 0x31, 0xc0, 0xb9, 0x08, 0x00, 0x00, 0x00,
            0x49, 0x03, 0x06, 0x48, 0x03, 0x44, 0x24, 0x08, 0xff, 0xc9, 0x75, 0xf4, 0x41, 0x5e, 0x5b, 0x41, 0x5f, 0xc3,
        }, 8 * (kA + kD)},
    };
}

std::vector<Leaf> backwardStubbedLeaves() {
    return {
        {"inflate_fast-shaped leaf comparing at rsp between an add and a jne", {
            0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x40,
            0x48, 0x89, 0x7c, 0x24, 0xe0, 0x48, 0x89, 0x7c, 0x24, 0x00, 0xb9, 0x04, 0x00, 0x00, 0x00, 0x31, 0xc0,
            0x48, 0x03, 0x44, 0x24, 0xe0, 0x49, 0x01, 0xf8, 0x48, 0x39, 0x3c, 0x24, 0x75, 0x03, 0x48, 0x01, 0xf0, 0xff, 0xc9, 0x75, 0xeb,
            0x48, 0x83, 0xc4, 0x40, 0x5b, 0x41, 0x5c, 0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
        }, 4 * (kA + kB)},
        {"leaf comparing at rsp after a load below rsp that heads its loop", {
            0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x40,
            0x48, 0x89, 0x7c, 0x24, 0xe0, 0x48, 0x89, 0x7c, 0x24, 0x00, 0xb9, 0x04, 0x00, 0x00, 0x00, 0x31, 0xc0,
            0x48, 0x03, 0x44, 0x24, 0xe0, 0x48, 0x39, 0x3c, 0x24, 0x75, 0x03, 0x48, 0x01, 0xf0, 0xff, 0xc9, 0x75, 0xee,
            0x48, 0x83, 0xc4, 0x40, 0x5b, 0x41, 0x5c, 0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
        }, 4 * (kA + kB)},
    };
}

void writeRel32(std::uint8_t* at, const std::uint64_t target, const std::uint64_t next) {
    const auto displacement = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(next);
    require(displacement >= std::numeric_limits<std::int32_t>::min() && displacement <= std::numeric_limits<std::int32_t>::max(), "a stub jump is out of rel32 range");
    const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
    for (std::size_t index = 0; index < 4; ++index) at[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

std::vector<std::uint8_t> callTo(const std::size_t from, const std::size_t target) {
    std::vector<std::uint8_t> bytes{0xe8, 0, 0, 0, 0};
    const auto displacement = static_cast<std::uint32_t>(static_cast<std::int32_t>(target) - static_cast<std::int32_t>(from + 5));
    for (std::size_t index = 0; index < 4; ++index) bytes[1 + index] = static_cast<std::uint8_t>(displacement >> (index * 8));
    return bytes;
}

struct Image {
    std::vector<std::uint8_t> Bytes;
    std::vector<std::size_t> Starts;
    std::vector<std::size_t> Ends;
};

Image place(const std::vector<Leaf>& leaves) {
    Image image;
    std::size_t at = 0x40;
    for (const auto& leaf : leaves) {
        image.Starts.push_back(at);
        at += (leaf.Code.size() + 15) / 16 * 16 + 16;
    }
    for (const auto start : image.Starts) {
        const auto call = callTo(image.Bytes.size(), start);
        image.Bytes.insert(image.Bytes.end(), call.begin(), call.end());
    }
    image.Bytes.push_back(0xc3);
    for (std::size_t index = 0; index < leaves.size(); ++index) {
        image.Bytes.resize(image.Starts[index], 0xcc);
        image.Bytes.insert(image.Bytes.end(), leaves[index].Code.begin(), leaves[index].Code.end());
        image.Ends.push_back(image.Bytes.size());
    }
    image.Bytes.resize((image.Bytes.size() + 15) / 16 * 16 + 16, 0xcc);
    require(image.Bytes.size() <= kStubBase, "the leaves do not fit before the stubs");
    return image;
}

struct Framing {
    Codegen::RedZoneFrameReport Report;
    std::vector<Codegen::TrampolineSite> Sites;
    std::vector<Codegen::RedZoneDisplacement> Displacements;
};

Framing load(const Image& image, const bool frame) {
    std::fill_n(arena, kArenaBytes, 0xcc);
    std::copy(image.Bytes.begin(), image.Bytes.end(), arena);
    std::copy(Runner.begin(), Runner.end(), arena + kRunnerBase);
    Framing framing;
    if (!frame) return framing;
    const auto base = reinterpret_cast<std::uint64_t>(arena);
    const std::vector<Domain::ProgramHeader> segments{{1, 5, 0, base, base, image.Bytes.size(), image.Bytes.size(), 16}};
    std::vector<Codegen::RedZoneFunction> functions;
    for (std::size_t index = 0; index < image.Starts.size(); ++index) functions.push_back({base + image.Starts[index], base + image.Ends[index]});
    framing.Sites = Codegen::FrameRedZoneFunctions(image.Bytes, segments, functions, {}, framing.Report, framing.Displacements);
    for (const auto& moved : framing.Displacements) std::copy(moved.Bytes.begin(), moved.Bytes.end(), arena + moved.Offset);
    std::size_t cursor = kStubBase;
    for (const auto& site : framing.Sites) {
        const auto at = static_cast<std::size_t>(site.Address - base);
        require(std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(), arena + at), "a framed site's bytes changed before it was patched");
        cursor = (cursor + 15) / 16 * 16;
        require(cursor + site.Body.size() <= kRunnerBase, "the stubs do not fit before the runner");
        auto* stub = arena + cursor;
        std::copy(site.Body.begin(), site.Body.end(), stub);
        Codegen::ApplyStubRelocations(std::span<std::uint8_t>(stub, site.ReturnBranchOffset), site.Relocations, site.Address, base + cursor, site.Offset);
        writeRel32(stub + site.ReturnBranchOffset + 1, site.Address + site.Length, base + cursor + site.ReturnBranchOffset + 5);
        std::fill_n(arena + at, site.Length, 0x90);
        arena[at] = 0xe9;
        writeRel32(arena + at + 1, base + cursor, site.Address + 5);
        cursor += site.Body.size();
    }
    return framing;
}

struct Run {
    std::uint64_t Result;
    int Steps;
};

Run run(const std::size_t start, const bool clobber) {
    using Entry = std::uint64_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
    const auto entry = reinterpret_cast<Entry>(arena + kRunnerBase);
    steps = 0;
    const auto result = entry(reinterpret_cast<std::uint64_t>(arena + start), kA, kB, kC, kD, clobber ? 1 : 0);
    return {result, static_cast<int>(steps)};
}

std::string hex(const std::uint64_t value) {
    std::ostringstream text;
    text << "0x" << std::hex << value;
    return text.str();
}

Codegen::RedZoneFrameReport runLeaves(const std::vector<Leaf>& leaves) {
    const auto image = place(leaves);
    load(image, false);
    std::vector<Run> plain, clobbered;
    for (std::size_t index = 0; index < leaves.size(); ++index) {
        plain.push_back(run(image.Starts[index], false));
        clobbered.push_back(run(image.Starts[index], true));
        require(plain.back().Result == leaves[index].Expected, std::string(leaves[index].Name) + ": the unframed leaf returned " + hex(plain.back().Result) + " without the clobber, not " + hex(leaves[index].Expected));
        require(clobbered.back().Steps > 0, std::string(leaves[index].Name) + ": the clobber did not run");
        require(clobbered.back().Result != leaves[index].Expected, std::string(leaves[index].Name) + ": the unframed leaf survived writes below rsp, so the case proves nothing");
    }
    const auto framing = load(image, true);
    require(framing.Report.Framed == leaves.size(), "only " + std::to_string(framing.Report.Framed) + " of " + std::to_string(leaves.size()) + " leaves were framed");
    for (std::size_t index = 0; index < leaves.size(); ++index) {
        const auto calm = run(image.Starts[index], false);
        const auto framed = run(image.Starts[index], true);
        require(calm.Result == leaves[index].Expected, std::string(leaves[index].Name) + ": the framed leaf returned " + hex(calm.Result) + " without the clobber, not " + hex(leaves[index].Expected));
        require(framed.Steps > 0, std::string(leaves[index].Name) + ": the clobber did not run over the framed leaf");
        require(framed.Result == leaves[index].Expected, std::string(leaves[index].Name) + ": the framed leaf returned " + hex(framed.Result) + " under writes below rsp, not " + hex(leaves[index].Expected));
        std::cout << leaves[index].Name << ": unframed " << hex(plain[index].Result) << " alone and " << hex(clobbered[index].Result) << " under the clobber (" << clobbered[index].Steps << " steps); framed " << hex(framed.Result) << " under the clobber (" << framed.Steps << " steps)\n";
    }
    return framing.Report;
}

}

int main() {
    try {
        struct sigaction action {};
        action.sa_sigaction = clobberBelowRsp;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        require(sigaction(SIGTRAP, &action, nullptr) == 0, "cannot install the SIGTRAP handler");
        void* mapped = mmap(nullptr, kArenaBytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped == MAP_FAILED) {
            std::cout << "skipped: no writable and executable memory\n";
            return 77;
        }
        arena = static_cast<std::uint8_t*>(mapped);
        runLeaves(framedLeaves());
        const auto stubbed = runLeaves(stubbedLeaves());
        require(stubbed.OperandStubs == 3 && stubbed.StubbedOperands == 5, "the leaves were framed with " + std::to_string(stubbed.OperandStubs) + " operand stubs carrying " + std::to_string(stubbed.StubbedOperands) + " rsp operands, not 3 carrying 5");
        const auto backward = runLeaves(backwardStubbedLeaves());
        require(backward.OperandStubs == 2 && backward.StubbedOperands == 3 && backward.MovedOperands == 5, "the leaves comparing at rsp before a branch were framed with " + std::to_string(backward.OperandStubs) + " operand stubs carrying " + std::to_string(backward.StubbedOperands) + " rsp operands and " + std::to_string(backward.MovedOperands) + " moved in place, not 2 carrying 3 and 5");
        std::cout << "red-zone framer execution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "red-zone framer execution tests failed: " << error.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cout << "skipped: the red-zone framer execution tests single-step x86-64 code on Linux\n";
    return 77;
}

#endif

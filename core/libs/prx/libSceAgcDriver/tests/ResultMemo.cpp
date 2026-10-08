#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ResultMemoAdmission.hpp"
#include <algorithm>
#include <barrier>
#include <chrono>
#include <cstring>
#include <deque>
#include <set>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
using namespace ShaderRecompiler;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Fixture {
    std::array<std::uint32_t, 8> code{0xf4040004u,0xfa000000u,0xf4000080u,0xfa000000u,0x7e000202u,0xf80008cfu,0u,0xbf810000u};
    std::uint32_t payload = 0x3f800000u;
    std::uint64_t table = reinterpret_cast<std::uintptr_t>(&payload);
    std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&table)),static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&table) >> 32u)};
    std::array<MemoryRegion, 2> memory{{{reinterpret_cast<std::uintptr_t>(&table),std::as_bytes(std::span(&table,1))},{reinterpret_cast<std::uintptr_t>(&payload),std::as_bytes(std::span(&payload,1))}}};
    RecompileRequest request{};
    ResourceCapture capture;
    std::size_t slot = 0;
    Fixture() {
        request.shader = {ShaderStage::Vertex,0x750000u,code,0,{}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.context.memory = memory;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.layout.pushConstantSizeBytes = 128;
        RequestMemoryView view(memory);
        capture = *CaptureResources(request,view.MakeRuntime(userData,request.shader.codeAddress));
        require(capture.readTrace.leaves.size() == 1,"memo fixture has no scalar data leaf");
        slot = capture.readTrace.leaves[0].first;
    }
    std::shared_ptr<const RecompileResult> Compile(std::uint32_t value, bool* hit = nullptr) {
        capture.snapshot.flattenedSrt.at(slot) = value;
        return Recompile(request,capture,hit);
    }
    std::uint32_t Value(const RecompileResult& result) const {
        for (const auto& binding : result.bindings) {
            if (binding.role == DescriptorRole::FlattenedSrt) return binding.guestDescriptor.at(slot);
        }
        throw std::runtime_error("memo fixture lost its flattened SRT binding");
    }
};
void admissionTests() {
    Detail::ResultMemoAdmission admission;
    require(!admission.Observe(0,0),"empty admission slot matched a zero key");
    require(admission.Observe(0,0),"repeated zero key was not admitted");
    const auto first = Detail::ResultMemoIndex(1,512);
    const auto second = Detail::ResultMemoIndex(2,1024);
    require(!admission.Observe(1,first),"new key was admitted");
    require(!admission.Observe(2,second),"colliding bucket admitted a different identity");
    require(admission.Observe(1,first),"bucket collision lost a recent identity");
    const auto sameIndex = Detail::ResultMemoIndex(2,512);
    require(!admission.Observe(2,sameIndex),"colliding hash index matched a different identity");
    require(admission.Observe(2,sameIndex),"full key repetition was not retained");
    for (bool clustered : {false,true}) {
        Detail::ResultMemoAdmission tested;
        std::set<std::pair<std::uint64_t,std::uint64_t>> reference;
        std::deque<std::pair<std::uint64_t,std::uint64_t>> order;
        std::uint64_t random = 0xb389042314916f21ull;
        for (std::size_t i = 0; i < 100000; ++i) {
            random ^= random << 13;
            random ^= random >> 7;
            random ^= random << 17;
            const auto variant = clustered ? random % 300 : random % 7;
            const auto hash = clustered ? Detail::ResultMemoIndex(variant,507) : (random >> 12) % 128;
            const auto key = std::pair{variant,hash};
            const bool expected = reference.contains(key);
            require(tested.Observe(variant,hash) == expected,"admission history disagrees with bounded FIFO reference");
            if (!expected) {
                order.push_back(key);
                reference.insert(key);
                if (order.size() > 256) { reference.erase(order.front());order.pop_front(); }
            }
        }
    }
}
void memoTests() {
    Fixture fixture;
    bool hit = true;
    auto first = fixture.Compile(0x41400000u,&hit);
    require(!hit && fixture.Value(*first) == 0x41400000u,"first result was stale or marked cached");
    std::weak_ptr<const RecompileResult> transient = first;
    first.reset();
    require(transient.expired(),"one-use result was retained by the memo");
    auto retained = fixture.Compile(0x41400000u,&hit);
    require(!hit,"second occurrence was already cached");
    auto repeat = fixture.Compile(0x41400000u,&hit);
    require(hit && repeat == retained,"repeated result was not memoized");
    for (std::uint32_t i = 0; i < 4096; ++i) {
        auto result = fixture.Compile(0x42000000u + i,&hit);
        require(!hit && fixture.Value(*result) == 0x42000000u + i,"one-use stream reused stale data");
    }
    require(fixture.Compile(0x41400000u,&hit) == retained && hit,"one-use stream evicted a reusable result");
    require(fixture.Value(*retained) == 0x41400000u,"retained result changed with later inputs");
    fixture.request.layout.pushConstantSizeBytes = 64;
    auto variant = fixture.Compile(0x41400000u,&hit);
    require(!hit && variant->variantId != retained->variantId,"different compiled variant reused a result");
    fixture.request.layout.pushConstantSizeBytes = 128;
    require(fixture.Compile(0x41400000u,&hit) == retained && hit,"variant switch lost a retained result");
    for (std::uint32_t i = 0; i < 300; ++i) {
        (void)fixture.Compile(0x43000000u + i);
        (void)fixture.Compile(0x43000000u + i);
    }
    (void)fixture.Compile(0x41400000u,&hit);
    require(!hit,"retained cache exceeded its eviction bound");
    require(fixture.Value(*retained) == 0x41400000u,"eviction invalidated an externally retained result");
    fixture.capture.snapshot.flattenedSrt.at(fixture.slot) = 0x44000000u;
    std::barrier start(8);
    std::array<std::future<std::shared_ptr<const RecompileResult>>,8> futures;
    for (auto& future : futures) future = std::async(std::launch::async,[&] { start.arrive_and_wait();return Recompile(fixture.request,fixture.capture); });
    for (auto& future : futures) require(fixture.Value(*future.get()) == 0x44000000u,"concurrent memo request received stale data");
    auto cached = fixture.Compile(0x44000000u,&hit);
    require(hit && fixture.Compile(0x44000000u) == cached,"concurrent misses failed to retain a stable result");
}
void benchmark() {
    std::thread([] {}).join();
    Fixture fixture;
    for (const bool hot : {false,true}) {
        std::array<double,9> times;
        std::uint64_t checksum = 0;
        std::uint64_t hits = 0;
        for (std::size_t pass = 0; pass <= times.size(); ++pass) {
            const auto start = std::chrono::steady_clock::now();
            for (std::uint32_t i = 0; i < 65536; ++i) {
                bool hit = false;
                auto result = fixture.Compile(hot ? 0x50000000u + i % 64 : 0x60000000u + pass * 65536 + i, &hit);
                hits += hit;
                checksum += fixture.Value(*result);
            }
            if (pass != 0) times[pass-1] = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/65536;
        }
        std::sort(times.begin(),times.end());
        std::cout << (hot ? "repeated64" : "one-use") << " median_us=" << times[4] << " hits=" << hits << " checksum=" << checksum << '\n';
    }
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--benchmark") benchmark();
        else { admissionTests();memoTests();std::cout << "result memo admission tests passed\n"; }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#include "prx/libSceXml/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct TestAllocator {
    XmlMemAllocator base;
    std::vector<void*> blocks;
    std::string log;
    std::size_t failAt = 0;
    std::size_t allocations = 0;
    void* expectedUserData = nullptr;
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == allocator->expectedUserData, "allocate receives the user data");
    ++allocator->allocations;
    allocator->log += "a" + std::to_string(size) + " ";
    if (allocator->allocations == allocator->failAt) return nullptr;
    void* block = std::malloc(size);
    std::memset(block, 0xcd, size);
    allocator->blocks.push_back(block);
    return block;
}

void APS5_VABI TestDeallocate(XmlMemAllocator* self, void* pointer, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == allocator->expectedUserData, "deallocate receives the user data");
    std::size_t index = 0;
    while (index < allocator->blocks.size() && allocator->blocks[index] != pointer) ++index;
    Require(index < allocator->blocks.size(), "deallocate receives a block it allocated");
    allocator->log += "f" + std::to_string(index) + " ";
    std::free(pointer);
    allocator->blocks[index] = nullptr;
}

void APS5_VABI TestDestroy(XmlMemAllocator*) {}

const XmlMemAllocatorVtable g_testVtable{TestDestroy, TestDestroy, TestAllocate, TestDeallocate};

struct Step {
    const char* label;
    int (*call)(XmlHandle*, const XmlInitParameter*);
    std::size_t failAt;
    int result;
    bool selfSet;
    const char* log;
};

int Initialize(XmlHandle* self, const XmlInitParameter* parameter) {
    return _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(self, parameter);
}

int Terminate(XmlHandle* self, const XmlInitParameter*) {
    return _ZN3sce3Xml11Initializer9terminateEv(self);
}

int Destroy(XmlHandle* self, const XmlInitParameter*) {
    _ZN3sce3Xml11InitializerD1Ev(self);
    return 0;
}

void TestMemAllocatorBase() {
    XmlMemAllocator allocator{nullptr};
    _ZN3sce3Xml12MemAllocatorC2Ev(&allocator);
    Require(allocator.vtable != nullptr, "the constructor installs the base vtable");
    Require(allocator.vtable->allocate != nullptr && allocator.vtable->deallocate != nullptr, "the base vtable fills the allocation slots");
    allocator.vtable->destroy(&allocator);
    _ZN3sce3Xml12MemAllocatorD1Ev(&allocator);
    bool threw = false;
    try {
        _ZN3sce3Xml12MemAllocatorD0Ev(&allocator);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "the base deleting destructor does not return");
}

void TestInitializer() {
    TestAllocator allocator{};
    allocator.base.vtable = &g_testVtable;
    void* const userData = reinterpret_cast<void*>(0x1234);
    allocator.expectedUserData = userData;
    const XmlInitParameter parameter{&allocator.base, userData, {0x10000, 0x7777}};
    XmlHandle initializer{nullptr};
    XmlHandle other{nullptr};
    Require(Initialize(&initializer, nullptr) == static_cast<int>(0x80850003) && initializer.impl == nullptr, "a null parameter is rejected");
    other.impl = reinterpret_cast<void*>(0x5555);
    Require(Initialize(&other, &parameter) == 0x00850003 && allocator.log.empty(), "an object that is already set is left alone");
    other.impl = nullptr;
    const Step steps[] = {
        {"first allocation fails", Initialize, 1, static_cast<int>(0x80850001), false, "a32 "},
        {"initialize", Initialize, 0, 0, true, "a32 a16 a200 "},
        {"second initializer while active", nullptr, 0, 0x00850003, false, ""},
        {"terminate", Terminate, 0, 0, false, "f2 f1 f0 "},
        {"terminate again", Terminate, 0, static_cast<int>(0x80850002), false, ""},
        {"initialize", Initialize, 0, 0, true, "a32 a16 a200 "},
        {"destructor after initialize", Destroy, 0, 0, false, "f5 f4 f3 "},
        {"destructor never initialized", Destroy, 0, 0, false, ""},
        {"third allocation fails", Initialize, 3, static_cast<int>(0x80851000), true, "a32 a16 a200 "},
        {"terminate after that", Terminate, 0, 0, false, "f7 f6 "},
        {"initialize", Initialize, 0, 0, true, "a32 a16 a200 "},
        {"terminate", Terminate, 0, 0, false, "f10 f9 f8 "},
        {"second allocation fails", Initialize, 2, static_cast<int>(0x80850001), false, "a32 a16 f11 "},
        {"initialize after that", Initialize, 0, 0, true, "a32 a16 "},
        {"terminate", Terminate, 0, 0, false, "f13 f12 "},
        {"initialize again", Initialize, 0, 0, true, "a32 a16 "},
        {"terminate", Terminate, 0, 0, false, "f15 f14 "},
    };
    for (const Step& step : steps) {
        allocator.log.clear();
        allocator.failAt = step.failAt == 0 ? 0 : allocator.allocations + step.failAt;
        int result;
        bool selfSet;
        if (step.call == nullptr) {
            result = Initialize(&other, &parameter);
            selfSet = other.impl != nullptr;
        } else {
            result = step.call(&initializer, &parameter);
            selfSet = initializer.impl != nullptr;
        }
        if (result != step.result || selfSet != step.selfSet || allocator.log != step.log) {
            std::fprintf(stderr, "%s: result %#x set %d log '%s'\n", step.label, static_cast<unsigned>(result), selfSet ? 1 : 0, allocator.log.c_str());
            Require(false, step.label);
        }
        if (step.selfSet && step.call == Initialize && step.result == 0) {
            Require(std::memcmp(initializer.impl, &parameter, sizeof(parameter)) == 0, "the implementation holds a copy of the parameter");
        }
    }
    for (void* block : allocator.blocks) {
        if (block != nullptr) std::free(block);
    }
}

struct SharedBlocks {
    std::vector<void*> blocks;
    std::string log;
};

struct NamedAllocator {
    XmlMemAllocator base;
    char name;
    void* userData;
    SharedBlocks* shared;
    std::size_t failAt = 0;
    std::size_t allocations = 0;
};

void* APS5_VABI NamedAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<NamedAllocator*>(self);
    Require(userData == allocator->userData, "allocate receives this allocator's user data");
    ++allocator->allocations;
    allocator->shared->log += std::string(1, allocator->name) + "a" + std::to_string(size) + " ";
    if (allocator->allocations == allocator->failAt) return nullptr;
    void* block = std::malloc(size);
    std::memset(block, 0xcd, size);
    allocator->shared->blocks.push_back(block);
    return block;
}

void APS5_VABI NamedDeallocate(XmlMemAllocator* self, void* pointer, void* userData) {
    auto* allocator = reinterpret_cast<NamedAllocator*>(self);
    Require(userData == allocator->userData, "deallocate receives this allocator's user data");
    std::size_t index = 0;
    while (index < allocator->shared->blocks.size() && allocator->shared->blocks[index] != pointer) ++index;
    Require(index < allocator->shared->blocks.size(), "deallocate receives a live block");
    allocator->shared->log += std::string(1, allocator->name) + "f" + std::to_string(index) + " ";
    std::free(pointer);
    allocator->shared->blocks[index] = nullptr;
}

const XmlMemAllocatorVtable g_namedVtable{TestDestroy, TestDestroy, NamedAllocate, NamedDeallocate};

void TestTwoAllocators() {
    SharedBlocks shared;
    NamedAllocator first{{&g_namedVtable}, 'A', reinterpret_cast<void*>(0x1234), &shared};
    NamedAllocator second{{&g_namedVtable}, 'B', reinterpret_cast<void*>(0x5678), &shared};
    const XmlInitParameter firstParameter{&first.base, first.userData, {0x10000, 0x7777}};
    const XmlInitParameter secondParameter{&second.base, second.userData, {0x10000, 0x7777}};
    XmlHandle a{nullptr};
    XmlHandle b{nullptr};
    struct Pair {
        const char* label;
        XmlHandle* self;
        const XmlInitParameter* parameter;
        bool initialize;
        std::size_t failAt;
        int result;
        bool selfSet;
        const char* log;
    };
    const Pair steps[] = {
        {"A third allocation fails", &a, &firstParameter, true, 3, static_cast<int>(0x80851000), true, "Aa32 Aa16 Aa200 "},
        {"B initializes without an engine block", &b, &secondParameter, true, 0, 0, true, "Ba32 Ba16 "},
        {"A frees B's wrapper with its own allocator", &a, &firstParameter, false, 0, 0, false, "Af3 Af0 "},
        {"B frees only its own block", &b, &secondParameter, false, 0, 0, false, "Bf2 "},
        {"B initializes with an engine block", &b, &secondParameter, true, 0, 0, true, "Ba32 Ba16 Ba200 "},
        {"B terminates", &b, &secondParameter, false, 0, 0, false, "Bf6 Bf5 Bf4 "},
    };
    for (const Pair& step : steps) {
        shared.log.clear();
        auto* allocator = step.parameter == &firstParameter ? &first : &second;
        allocator->failAt = step.failAt == 0 ? 0 : allocator->allocations + step.failAt;
        const int result = step.initialize ? Initialize(step.self, step.parameter) : Terminate(step.self, step.parameter);
        const bool selfSet = step.self->impl != nullptr;
        if (result != step.result || selfSet != step.selfSet || shared.log != step.log) {
            std::fprintf(stderr, "%s: result %#x set %d log '%s'\n", step.label, static_cast<unsigned>(result), selfSet ? 1 : 0, shared.log.c_str());
            Require(false, step.label);
        }
    }
    for (void* block : shared.blocks) {
        if (block != nullptr) std::free(block);
    }
}

void TestInitializerNullValidation() {
    bool threwNullSelf = false;
    try {
        _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threwNullSelf = true;
    }
    Require(threwNullSelf, "Initializer::initialize rejects null self");

    XmlHandle handle{reinterpret_cast<void*>(0x1234)};
    _ZN3sce3Xml11InitializerC1Ev(&handle);
    Require(handle.impl == nullptr, "Initializer C1 clears impl");

    handle.impl = reinterpret_cast<void*>(0x1234);
    _ZN3sce3Xml11InitializerC2Ev(&handle);
    Require(handle.impl == nullptr, "Initializer C2 clears impl");

    XmlInitParameter paramWithoutAllocator{nullptr, nullptr, {0, 0}};
    bool threwNullAllocator = false;
    try {
        _ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&handle, &paramWithoutAllocator);
    } catch (const std::invalid_argument&) {
        threwNullAllocator = true;
    }
    Require(threwNullAllocator, "Initializer::initialize rejects null allocator");

    errno = 71;
    Require(_ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&handle, reinterpret_cast<const XmlInitParameter*>(0x10)) == static_cast<int>(0x80850003), "Initializer::initialize rejects unreadable parameter");
    Require(errno == 71, "Initializer::initialize preserves errno on unreadable parameter");

    bool threwTerminateNull = false;
    try {
        _ZN3sce3Xml11Initializer9terminateEv(nullptr);
    } catch (const std::invalid_argument&) {
        threwTerminateNull = true;
    }
    Require(threwTerminateNull, "Initializer::terminate rejects null self");

    bool threwDtorNull = false;
    try {
        _ZN3sce3Xml11InitializerD1Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threwDtorNull = true;
    }
    Require(threwDtorNull, "Initializer D1 rejects null self");

    bool threwDtor2Null = false;
    try {
        _ZN3sce3Xml11InitializerD2Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threwDtor2Null = true;
    }
    Require(threwDtor2Null, "Initializer D2 rejects null self");
}

void TestSimpleDataConstructors() {
    XmlSimpleData data1;
    _ZN3sce3Xml10SimpleDataC1Ev(&data1);
    Require(data1.data == nullptr && data1.size == 0, "SimpleData C1 default clears members");

    XmlSimpleData data2;
    _ZN3sce3Xml10SimpleDataC2Ev(&data2);
    Require(data2.data == nullptr && data2.size == 0, "SimpleData C2 default clears members");

    const char* sample = "hello";
    XmlSimpleData data3;
    _ZN3sce3Xml10SimpleDataC1EPKcm(&data3, sample, 5);
    Require(data3.data == sample && data3.size == 5, "SimpleData C1 param initializes members");

    XmlSimpleData data4;
    _ZN3sce3Xml10SimpleDataC2EPKcm(&data4, sample, 5);
    Require(data4.data == sample && data4.size == 5, "SimpleData C2 param initializes members");
}

}

int main() {
    TestMemAllocatorBase();
    TestInitializerNullValidation();
    TestSimpleDataConstructors();
    TestTwoAllocators();
    TestInitializer();
    std::puts("guest_xml_initializer: all checks passed");
    return 0;
}

#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using SelfCall = int (APS5_VABI*)(void*);
using Size = std::size_t (APS5_VABI*)(const void*);
using Element = void* (APS5_VABI*)(void*, std::uint64_t);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

static constexpr int ParameterNotInitialized = static_cast<int>(0x80553502);
static constexpr int ParameterInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ParameterAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ParameterTerminatedUninitialized = static_cast<int>(0x80553510);

struct ParameterLifecycle {
    int (APS5_VABI* construct)(void*);
    int (APS5_VABI* destroy)(void*);
    int (APS5_VABI* terminate)(void*);
    int (APS5_VABI* setText)(void*, const char*);
};

template <typename TInitialize>
static void TestParameterLifecycle(const ParameterLifecycle& lifecycle, TInitialize initialize) {
    std::uint64_t context = 0;
    alignas(16) unsigned char parameter[0x20];
    std::memset(parameter, 0x5a, sizeof(parameter));
    Require(lifecycle.construct(parameter) == 0, "a parameter constructor failed");
    for (std::size_t index = 0; index < sizeof(parameter); ++index)
        Require(parameter[index] == (index < 0x10 ? 0 : 0x5a), "a parameter constructor wrote the wrong bytes");
    Require(lifecycle.setText(parameter, "text") == ParameterNotInitialized, "a setter accepted an uninitialized parameter");
    Require(lifecycle.terminate(parameter) == ParameterTerminatedUninitialized, "terminate accepted an uninitialized parameter");
    Require(initialize(parameter, nullptr, true) == ParameterInvalidArgument, "initialize accepted a null context");
    Require(initialize(parameter, &context, true) == ParameterNotInitialized, "initialize accepted an uninitialized context");
    Require(_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(nullptr, &context) == 0, "the context did not initialize");
    Require(initialize(parameter, &context, false) == ParameterInvalidArgument, "initialize accepted a null argument");
    Require(lifecycle.setText(parameter, "text") == ParameterNotInitialized, "a failed initialize left the parameter initialized");
    Require(initialize(parameter, &context, true) == 0, "initialize failed");
    Require(initialize(parameter, &context, true) == ParameterAlreadyInitialized, "initialize ran twice");
    Require(lifecycle.setText(parameter, nullptr) == ParameterInvalidArgument, "a setter accepted a null string");
    Require(lifecycle.setText(parameter, "text") == 0, "a setter failed");
    Require(lifecycle.terminate(parameter) == 0, "terminate failed");
    Require(lifecycle.terminate(parameter) == ParameterTerminatedUninitialized, "terminate ran twice");
    Require(initialize(parameter, &context, true) == 0, "initialize after terminate failed");
    Require(lifecycle.terminate(parameter) == 0, "the second terminate failed");
    Require(lifecycle.destroy(parameter) == 0, "a parameter destructor failed");
    for (std::size_t index = 0; index < sizeof(parameter); ++index)
        Require(parameter[index] == (index < 0x10 ? 0 : 0x5a), "a parameter lifecycle wrote the wrong bytes");
    Require(_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(&context) == 0, "the context did not terminate");
}

static void TestParameterLifecycles() {
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainerC1Ev,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainerD1Ev,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer9terminateEv,
            _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setcontainerIdsEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            const bool before = _ZNK3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer13isInitializedEv(parameter);
            const int result = present ? _ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer10initializeEPNS1_6Common10LibContextE(parameter, context) : ParameterInvalidArgument;
            Require(_ZNK3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer13isInitializedEv(parameter) == (before || result == 0), "isInitialized disagrees with initialize");
            Require(_ZN3sce2Np9CppWebApi13InGameCatalog2V512ContainerApi23ParameterToGetContainer15setserviceLabelEi(parameter, 1) == 0, "setserviceLabel failed");
            return result;
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserC1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUserD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9terminateEv,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser9setfieldsEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi37ParameterToGetMultiDataStatusesByUser10initializeEPNS1_6Common10LibContextEPKcSA_(parameter, context, "me", present ? "slot" : nullptr);
        });
    TestParameterLifecycle(
        {
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataC1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadDataD1Ev,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData9terminateEv,
            _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData11setobjectIdEPKc,
        },
        [](void* parameter, const std::uint64_t* context, bool present) {
            return _ZN3sce2Np9CppWebApi17TitleCloudStorage2V17DataApi23ParameterToDownloadData10initializeEPNS1_6Common10LibContextEPKci(parameter, context, present ? "me" : nullptr, 0);
        });
}

static void TestIteratorDestructors() {
    unsigned char iterator[0x20];
    std::memset(iterator, 0x5a, sizeof(iterator));
    const SelfCall destructors[] = {
        _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_13InGameCatalog2V57ProductEEEED2Ev,
        _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_17TitleCloudStorage2V110DataStatusEEEED2Ev,
    };
    for (const auto call : destructors) Require(call(iterator) == 0, "an iterator destructor failed");
    for (const auto byte : iterator) Require(byte == 0x5a, "an iterator destructor wrote to the iterator");
}

static const Size sizes[] = {
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V53SkuEEEE4sizeEv,
    _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEE4sizeEv,
};

static const Element elements[] = {
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_13InGameCatalog2V59ContainerEEEEixEm,
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V113PlayerSessionEEEEixEm,
};

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void TestVectorSizeAndElements() {
    std::uint64_t context = 1;
    IntrusivePtr vector{nullptr, nullptr, &context};
    for (const auto size : sizes) Require(size(&vector) == 0, "a vector without a body has elements");
    for (const auto element : elements) Require(Throws([&] { element(&vector, 0); }), "indexing a vector without a body did not throw");

    alignas(8) std::int32_t first[4] = {1};
    alignas(8) std::int32_t second[4] = {1};
    const IntrusivePtr pushed[] = {{first, nullptr, &context}, {second, nullptr, &context}};
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEEC1EPNS2_10LibContextE(&vector, &context);
    for (const auto& element : pushed)
        Require(_ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEE8pushBackERKS8_(&vector, &element) == 0, "pushBack failed");
    for (const auto size : sizes) Require(size(&vector) == 2, "a vector reported the wrong size");
    for (const auto element : elements) {
        Require(static_cast<IntrusivePtr*>(element(&vector, 0))->object == first, "operator[] returned the wrong first element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1))->object == second, "operator[] returned the wrong second element");
        Require(static_cast<IntrusivePtr*>(element(&vector, 1)) == static_cast<IntrusivePtr*>(element(&vector, 0)) + 1, "operator[] elements are not contiguous");
    }
    _ZN3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V112JoinableUserEEEED1Ev(&vector);
    Require(first[0] == 1 && second[0] == 1, "destroying the vector did not release its references");

    alignas(8) unsigned char foreignBody[0x50] = {};
    IntrusivePtr foreign{foreignBody, nullptr, &context};
    for (const auto size : sizes) Require(Throws([&] { size(&foreign); }), "sizing a foreign body did not throw");
    for (const auto element : elements) Require(Throws([&] { element(&foreign, 0); }), "indexing a foreign body did not throw");
}

int main() {
    TestParameterLifecycles();
    TestIteratorDestructors();
    TestVectorSizeAndElements();
    std::puts("NpCppWebApi offline tests passed");
    return 0;
}

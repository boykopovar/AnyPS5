extern "C" {
#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN = static_cast<int>(0x80552907);
static constexpr std::size_t ReferenceCountOffset = 0;
static constexpr std::size_t VectorReferenceCountOffset = 0x30;
static constexpr int TransactionNotStarted = static_cast<int>(0x80553508);

// Offline PSN: request entry points (API calls, transaction start/readData, factories) fail with
// SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN, the constant shadPS4/fpPS4 document for this code; parameter
// bookkeeping succeeds; the audited accessors read the measured offsets and the rest stay
// unimplemented because a failed request never produces a response.

static void ConstructParameter(void* self);
static int InitializeParameter(void* self, const std::uint64_t* context, bool argumentsPresent);
static int InitializeParameterWithBody(void* self, const std::uint64_t* context, bool textsPresent, const IntrusivePtr* body);
static int TerminateParameter(void* self);

static constexpr int ParameterNotInitialized = static_cast<int>(0x80553502);
static constexpr int ParameterInvalidArgument = static_cast<int>(0x80553503);
static constexpr int ParameterAlreadyInitialized = static_cast<int>(0x8055350f);
static constexpr int ParameterTerminatedUninitialized = static_cast<int>(0x80553510);
static constexpr int ParameterBodyMissing = static_cast<int>(0x80553512);

struct ParameterObject {
    void* base;
    const void* implementation;
};

static constexpr unsigned char ParameterImplementation = 0;

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMinScoreLimitEv(const void* self) {
    return *reinterpret_cast<const std::int64_t*>(static_cast<const std::uint8_t*>(self) + 0x60);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18maxScoreLimitIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x58] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18minScoreLimitIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x68] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody20getLargeDataNumLimitEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x38);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody21getLargeDataSizeLimitEv(const void* self) {
    return *reinterpret_cast<const std::size_t*>(static_cast<const std::uint8_t*>(self) + 0x40);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody22largeDataNumLimitIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x3c] != 0;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody23largeDataSizeLimitIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x48] != 0;
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry10getCommentEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x18);
}

const CommonString* APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry11getObjectIdEv(const void* self) {
    return reinterpret_cast<const CommonString*>(static_cast<const std::byte*>(self) + 0x48);
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry11getOnlineIdEv(void* out, const void* self) {
    return CopyOnlineId(out, self, 0x69);
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12commentIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x38] != 0;
}

std::uint64_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12getAccountIdEv(const void* self) {
    return *reinterpret_cast<const std::uint64_t*>(static_cast<const std::uint8_t*>(self) + 0x10);
}

const CommonBinary* APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry12getSmallDataEv(const void* self) {
    return reinterpret_cast<const CommonBinary*>(static_cast<const std::byte*>(self) + 0xa0);
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13getSerialRankEv(const void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::byte*>(self) + 0x98, sizeof(value));
    return value;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry13objectIdIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0x68] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14getHighestRankEv(const void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::byte*>(self) + 0x3c, sizeof(value));
    return value;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry14smallDataIsSetEv(const void* self) {
    return static_cast<const std::uint8_t*>(self)[0xb8] != 0;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry20getHighestSerialRankEv(const void* self) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::byte*>(self) + 0x40, sizeof(value));
    return value;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry7getPcIdEv(const void* self) {
    std::int32_t value;
    std::memcpy(&value, static_cast<const std::byte*>(self) + 0x80, sizeof(value));
    return value;
}

std::uint32_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry7getRankEv(const void* self) {
    return *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(self) + 0x88);
}

std::int64_t APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V15Entry8getScoreEv(const void* self) {
    return *reinterpret_cast<const std::int64_t*>(static_cast<const std::uint8_t*>(self) + 0x90);
}

const char* APS5_VABI _ZNK3sce2Np9CppWebApi12Leaderboards2V19RecordApi26RecordScoreResponseHeaders24getXPsnAtomicOperationIdEv(const void* self) {
    const char* value = *reinterpret_cast<char* const*>(static_cast<const std::uint8_t*>(self) + 0x10);
    return value != nullptr ? value : "";
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V121GetRankingRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122GetRankingResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V122RecordScoreRequestBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V130GetBoardDefinitionResponseBodyEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V14UserEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V15EntryEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_12Leaderboards2V19RecordApi26RecordScoreResponseHeadersEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6BinaryEEptEv(const IntrusivePtr* self) {
    return self->object;
}

void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_12Leaderboards2V15EntryEEEEEEdeEv(const IntrusivePtr* self) {
    return self->object;
}

int APS5_VABI _ZNK3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_12Leaderboards2V123RecordScoreResponseBodyEEENS4_INS6_9RecordApi26RecordScoreResponseHeadersEEEE18getResponseHeadersERSB_() {
 return SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN;
}

static constexpr int BinaryInvalidArgument = static_cast<int>(0x80553503);
static constexpr int BinaryOutOfMemory = static_cast<int>(0x80553504);

static void ResetBinary(CommonBinary* self) {
    std::free(self->buffer);
    self->buffer = nullptr;
    self->size = 0;
}

static int SetBinary(CommonBinary* self, const void* data, const std::uint64_t size) {
    if (size == 0) {
        ResetBinary(self);
        return 0;
    }
    if (data == nullptr) return BinaryInvalidArgument;
    if (size > SIZE_MAX) return BinaryOutOfMemory;
    void* buffer = std::malloc(static_cast<std::size_t>(size));
    if (buffer == nullptr) return BinaryOutOfMemory;
    std::memcpy(buffer, data, static_cast<std::size_t>(size));
    std::free(self->buffer);
    self->buffer = buffer;
    self->size = size;
    return 0;
}

static int AppendBinary(CommonBinary* self, const void* data, const std::uint64_t size) {
    if (size == 0) return 0;
    if (data == nullptr) return BinaryInvalidArgument;
    if (size > UINT64_MAX - self->size) return BinaryOutOfMemory;
    const std::uint64_t total = self->size + size;
    if (total > SIZE_MAX) return BinaryOutOfMemory;
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(total)));
    if (buffer == nullptr) return BinaryOutOfMemory;
    if (self->size != 0) std::memcpy(buffer, self->buffer, static_cast<std::size_t>(self->size));
    std::memcpy(buffer + static_cast<std::size_t>(self->size), data, static_cast<std::size_t>(size));
    std::free(self->buffer);
    self->buffer = buffer;
    self->size = total;
    return 0;
}

static_assert(sizeof(CommonBinary) == 0x20 && offsetof(CommonBinary, buffer) == 0x08);
static_assert(offsetof(CommonBinary, size) == 0x10 && offsetof(CommonBinary, libContext) == 0x18);

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6BinaryC1EPNS2_10LibContextE(CommonBinary* self, void* libContext) {
    self->referenceCount = 0;
    self->buffer = nullptr;
    self->size = 0;
    self->libContext = libContext;
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6BinaryC2EPNS2_10LibContextE(CommonBinary* self, void* libContext) {
    _ZN3sce2Np9CppWebApi6Common6BinaryC1EPNS2_10LibContextE(self, libContext);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6BinaryD1Ev(CommonBinary* self) {
    ResetBinary(self);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6BinaryD2Ev(CommonBinary* self) {
    ResetBinary(self);
}

void APS5_VABI _ZN3sce2Np9CppWebApi6Common6Binary5clearEv(CommonBinary* self) {
    ResetBinary(self);
}

std::int32_t APS5_VABI _ZN3sce2Np9CppWebApi6Common6Binary21intrusive_ptr_add_refEPS3_(CommonBinary* self) {
    return std::atomic_ref<std::int32_t>(self->referenceCount).fetch_add(1) + 1;
}

std::int32_t APS5_VABI _ZN3sce2Np9CppWebApi6Common6Binary21intrusive_ptr_sub_refEPS3_(CommonBinary* self) {
    return std::atomic_ref<std::int32_t>(self->referenceCount).fetch_sub(1);
}

std::size_t APS5_VABI _ZNK3sce2Np9CppWebApi6Common6Binary4sizeEv(const CommonBinary* self) {
    return self->size;
}

const void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common6Binary9getBinaryEv(const CommonBinary* self) {
    return self->buffer;
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6Binary9setBinaryEPKvm(CommonBinary* self, const void* data, const std::uint64_t size) {
    return SetBinary(self, data, size);
}

int APS5_VABI _ZN3sce2Np9CppWebApi6Common6Binary12appendBinaryEPKvm(CommonBinary* self, const void* data, const std::uint64_t size) {
    return AppendBinary(self, data, size);
}


void* APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEEdeEv(const void* self) {
    return static_cast<const CommonIterator*>(self)->element;
}

bool APS5_VABI _ZNK3sce2Np9CppWebApi6Common8IteratorINS2_12IntrusivePtrINS1_12Leaderboards2V15EntryEEEEneERKS9_(const void* self, const void* other) {
    return HostIteratorNotEqual(self, other);
}

// Shared vector bodies. A Vector handle is an IntrusivePtr whose object is a body;
// bodies adopted from a title through the pointer/deleter constructor keep their
// 32-bit reference count at 0x30, so bodies created here do the same.
static constexpr std::uint64_t VectorBodyMagic = 0x434e507056656342ULL;
struct VectorBody {
    std::uint64_t magic;
    void* libContext;
    std::uint8_t reserved[0x20];
    std::int32_t referenceCount;
    std::int32_t elementKind;  // 1 = IntrusivePtr elements, 2 = String elements
    void* elements;
    std::uint64_t size;
    std::uint64_t capacity;
};
static_assert(sizeof(VectorBody) == 0x50 && offsetof(VectorBody, referenceCount) == 0x30);

static void APS5_VABI DestroyVectorBody(void* object) {
    auto* body = static_cast<VectorBody*>(object);
    if (body->elementKind == 1) {
        auto* elements = static_cast<IntrusivePtr*>(body->elements);
        for (std::uint64_t index = 0; index < body->size; ++index) {
            ReleaseIntrusivePtr(&elements[index], 0x00, __func__);
        }
    } else if (body->elementKind == 2) {
        auto* elements = static_cast<CommonString*>(body->elements);
        for (std::uint64_t index = 0; index < body->size; ++index) {
            std::free(elements[index].buffer);
        }
    }
    std::free(body->elements);
    std::free(body);
}

static VectorBody* VectorBodyFor(IntrusivePtr* self, const std::int32_t elementKind, const char* function) {
    auto* direct = static_cast<VectorBody*>(static_cast<void*>(self));
    if (direct != nullptr && direct->magic == VectorBodyMagic) {
        if (direct->elementKind != elementKind) {
            NotImplemented_nid_no_patch(function);
            return nullptr;
        }
        return direct;
    }
    auto* body = static_cast<VectorBody*>(self->object);
    if (body == nullptr) {
        body = static_cast<VectorBody*>(std::calloc(1, sizeof(VectorBody)));
        if (body == nullptr) return nullptr;
        body->magic = VectorBodyMagic;
        body->referenceCount = 1;
        body->elementKind = elementKind;
        body->libContext = self->libContext;
        self->object = body;
        self->deleter = &DestroyVectorBody;
        return body;
    }
    if (body->magic != VectorBodyMagic || body->elementKind != elementKind) {
        // appending to a title-provided body needs its element layout, which is unknown
        NotImplemented_nid_no_patch(function);
        return nullptr;
    }
    return body;
}

static bool VectorGrow(VectorBody* body, const std::size_t elementSize) {
    if (body->size < body->capacity) return true;
    const std::uint64_t capacity = body->capacity != 0 ? body->capacity * 2 : 4;
    void* elements = std::realloc(body->elements, static_cast<std::size_t>(capacity) * elementSize);
    if (elements == nullptr) return false;
    body->elements = elements;
    body->capacity = capacity;
    return true;
}

static const VectorBody* ResolveVectorBody(const void* self) {
    const auto* direct = static_cast<const VectorBody*>(self);
    if (direct == nullptr) return nullptr;
    if (direct->magic == VectorBodyMagic) return direct;
    return static_cast<const VectorBody*>(static_cast<const IntrusivePtr*>(self)->object);
}

static std::uint64_t HostVectorSize(const void* self, const char* function) {
    const auto* body = ResolveVectorBody(self);
    if (body == nullptr) return 0;
    if (body->magic != VectorBodyMagic) NotImplemented_nid_no_patch(function);
    return body->size;
}

static IntrusivePtr* HostVectorElement(void* self, const std::uint64_t index, const char* function) {
    auto* body = const_cast<VectorBody*>(ResolveVectorBody(self));
    if (body == nullptr || body->magic != VectorBodyMagic || body->elementKind != 1) NotImplemented_nid_no_patch(function);
    return static_cast<IntrusivePtr*>(body->elements) + index;
}

static int VectorPushBackIntrusivePtr(IntrusivePtr* self, const IntrusivePtr* element) {
    VectorBody* body = VectorBodyFor(self, 1, __func__);
    if (body == nullptr) return -1;
    const IntrusivePtr copy = *element;
    if (!VectorGrow(body, sizeof(IntrusivePtr))) return -1;
    auto* elements = static_cast<IntrusivePtr*>(body->elements);
    elements[body->size] = copy;
    if (copy.object != nullptr) ReferenceCount(copy.object, ReferenceCountOffset).fetch_add(1);
    body->size += 1;
    return 0;
}

static int VectorPushBackString(IntrusivePtr* self, const CommonString* element) {
    VectorBody* body = VectorBodyFor(self, 2, __func__);
    if (body == nullptr) return -1;
    CommonString copy = *element;
    copy.referenceCount = 0;
    copy.buffer = nullptr;
    copy.bufferSize = 0;
    if (element->buffer != nullptr) {
        const std::size_t length = std::strlen(element->buffer) + 1;
        copy.buffer = static_cast<char*>(std::malloc(length));
        if (copy.buffer == nullptr) return -1;
        std::memcpy(copy.buffer, element->buffer, length);
        copy.bufferSize = length;
    }
    if (!VectorGrow(body, sizeof(CommonString))) {
        std::free(copy.buffer);
        return -1;
    }
    static_cast<CommonString*>(body->elements)[body->size] = copy;
    body->size += 1;
    return 0;
}

static_assert(sizeof(CommonIterator) == 0x10 && offsetof(CommonIterator, element) == 0x08);

static IntrusivePtr* IntrusiveVectorElements(const IntrusivePtr* self, const bool end, const char* function) {
    const auto* body = static_cast<const VectorBody*>(self->object);
    if (body == nullptr) return nullptr;
    if (body->magic != VectorBodyMagic || body->elementKind != 1) NotImplemented_nid_no_patch(function);
    auto* elements = static_cast<IntrusivePtr*>(body->elements);
    return end && body->size != 0 ? elements + body->size : elements;
}

static CommonString* StringVectorElements(const IntrusivePtr* self, const bool end, const char* function) {
    const auto* body = static_cast<const VectorBody*>(self->object);
    if (body == nullptr) return nullptr;
    if (body->magic != VectorBodyMagic || body->elementKind != 2) NotImplemented_nid_no_patch(function);
    auto* elements = static_cast<CommonString*>(body->elements);
    return end && body->size != 0 ? elements + body->size : elements;
}

static const VectorBody* HostVectorBody(const void* self, const std::int32_t elementKind, const char* function) {
    const auto* body = ResolveVectorBody(self);
    if (body == nullptr) return nullptr;
    if (body->magic != VectorBodyMagic || body->elementKind != elementKind) NotImplemented_nid_no_patch(function);
    return body;
}

static void* HostVectorBegin(void* out, const void* self, const std::int32_t elementKind, const char* function) {
    const VectorBody* body = HostVectorBody(self, elementKind, function);
    auto* it = static_cast<CommonIterator*>(out);
    it->referenceCount = 0;
    it->element = body != nullptr ? body->elements : nullptr;
    return out;
}

static void* HostVectorEnd(void* out, const void* self, const std::int32_t elementKind, const std::size_t elementSize, const char* function) {
    const VectorBody* body = HostVectorBody(self, elementKind, function);
    auto* it = static_cast<CommonIterator*>(out);
    it->referenceCount = 0;
    it->element = body != nullptr && body->elements != nullptr ? static_cast<std::uint8_t*>(body->elements) + body->size * elementSize : nullptr;
    return out;
}

static void* HostIteratorAdvance(void* self, const std::size_t elementSize) {
    auto* it = static_cast<CommonIterator*>(self);
    it->element = static_cast<std::uint8_t*>(it->element) + elementSize;
    return self;
}

static bool HostIteratorNotEqual(const void* self, const void* other) {
    return static_cast<const CommonIterator*>(self)->element != static_cast<const CommonIterator*>(other)->element;
}

static bool VectorReserve(VectorBody* body, const std::size_t elementSize, const std::uint64_t count) {
    if (body->capacity >= count) return true;
    std::uint64_t capacity = body->capacity != 0 ? body->capacity : 4;
    while (capacity < count) capacity *= 2;
    void* elements = std::realloc(body->elements, static_cast<std::size_t>(capacity) * elementSize);
    if (elements == nullptr) return false;
    body->elements = elements;
    body->capacity = capacity;
    return true;
}

static int HostVectorCopyFrom(IntrusivePtr* self, const IntrusivePtr* source, const char* function) {
    VectorBody* body = VectorBodyFor(self, 1, function);
    if (body == nullptr) return ParameterOutOfMemory;
    const auto* sourceDirect = static_cast<const VectorBody*>(static_cast<const void*>(source));
    const bool sourceIsBody = sourceDirect != nullptr && sourceDirect->magic == VectorBodyMagic;
    const auto* sourceBody = sourceIsBody ? sourceDirect : static_cast<const VectorBody*>(source->object);
    void** const selfContext = body == static_cast<VectorBody*>(static_cast<void*>(self)) ? &body->libContext : &self->libContext;
    void* const sourceContext = sourceIsBody ? sourceDirect->libContext : source->libContext;
    if (*selfContext == nullptr) *selfContext = sourceContext;
    if (body->libContext == nullptr) body->libContext = *selfContext;
    if (sourceBody != nullptr && (sourceBody->magic != VectorBodyMagic || sourceBody->elementKind != 1)) {
        NotImplemented_nid_no_patch(function);
        return ParameterOutOfMemory;
    }
    if (sourceBody != nullptr && body == sourceBody) return 0;
    auto* elements = static_cast<IntrusivePtr*>(body->elements);
    for (std::uint64_t index = 0; index < body->size; ++index) {
        ReleaseIntrusivePtr(&elements[index], ReferenceCountOffset, function);
    }
    body->size = 0;
    const std::uint64_t count = sourceBody != nullptr ? sourceBody->size : 0;
    if (count == 0) return 0;
    if (!VectorReserve(body, sizeof(IntrusivePtr), count)) return ParameterOutOfMemory;
    elements = static_cast<IntrusivePtr*>(body->elements);
    const auto* sourceElements = static_cast<const IntrusivePtr*>(sourceBody->elements);
    for (std::uint64_t index = 0; index < count; ++index) {
        elements[index] = {};
        CopyIntrusivePtr(&elements[index], &sourceElements[index], ReferenceCountOffset);
    }
    body->size = count;
    return 0;
}

}

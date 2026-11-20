#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::FormDataElement;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::EmptyStringImpl;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::Vector;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase ops check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<class TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static std::map<void*, std::size_t> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations[pointer] = size;
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static std::string Text(const String& value) {
    if (!value.impl) return "<null>";
    Require((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

static std::string TextOf(const StringImpl* impl) {
    if (!impl) return "<null>";
    Require((impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(impl->data), impl->length);
}

static HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

static Vector<String>& FallbackArray(ResourceRequestBase& request) {
    return *reinterpret_cast<Vector<String>*>(&request.responseContentDispositionEncodingFallbackArray);
}

static ResourceRequestBase MakeRequest() {
    ResourceRequestBase request{};
    request.priority = 1;
    request.allowCookies = true;
    request.isAppInitiated = true;
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    return request;
}

static void Clear(ResourceRequestBase& request) {
    auto& map = Headers(request);
    while (map.common.size) _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&map, map.common.buffer[0].key);
    while (map.uncommon.size) {
        String key(map.uncommon.buffer[0].key);
        _ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE(&map, key);
    }
    _ZN3WTF8fastFreeEPv(map.common.buffer);
    _ZN3WTF8fastFreeEPv(map.uncommon.buffer);
    map = HTTPHeaderMap{};
}

static FormData* CreateForm(const void* data, std::size_t size) {
    auto ref = _ZN7WebCore8FormData6createEPKvm(data, size);
    return std::exchange(ref.m_ptr, nullptr);
}

static FormData* CreateEmptyForm() {
    auto ref = _ZN7WebCore8FormData6createEv();
    return std::exchange(ref.m_ptr, nullptr);
}

static void FreeForm(FormData* form) {
    _ZN7WebCore8FormDataD1Ev(form);
    _ZdlPv_nid_postfix(form);
}

static std::string BodyBytes(const FormDataElement& element) {
    return std::string(reinterpret_cast<const char*>(element.storage.bytes.buffer), element.storage.bytes.size);
}

static FormDataElement& Push(FormData* form) {
    auto& elements = form->elements;
    if (elements.size == elements.capacity) {
        const unsigned capacity = elements.capacity ? elements.capacity * 2 : 4;
        auto* buffer = static_cast<FormDataElement*>(_ZN3WTF10fastMallocEm(capacity * sizeof(FormDataElement)));
        if (elements.size) std::memcpy(buffer, elements.buffer, elements.size * sizeof(FormDataElement));
        _ZN3WTF8fastFreeEPv(elements.buffer);
        elements.buffer = buffer;
        elements.capacity = capacity;
    }
    return elements.buffer[elements.size++];
}

static void PushFile(FormData* form, const String& filename, std::int64_t start, std::int64_t length, bool hasTime = false, double time = 0) {
    auto& element = Push(form);
    std::memset(&element, 0, sizeof(element));
    element.storage.file.filename = filename.impl;
    filename.impl->refCount += 2;
    element.storage.file.fileStart = start;
    element.storage.file.fileLength = length;
    element.storage.file.hasExpectedFileModificationTime = hasTime;
    element.storage.file.expectedFileModificationTime = time;
    element.index = 1;
}

static void PushValueless(FormData* form) {
    auto& element = Push(form);
    std::memset(&element, 0, sizeof(element));
    element.index = 0xFFFFFFFF;
}

static void ReleaseRequest(ResourceRequestBase& request) {
    NKWebKit::WtfAbi::Release(request.url.impl);
    NKWebKit::WtfAbi::Release(request.firstPartyForCookies.impl);
    Clear(request);
    auto& vector = FallbackArray(request);
    for (unsigned i = 0; i < vector.size; ++i) NKWebKit::WtfAbi::Release(vector.buffer[i].impl);
    if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = nullptr;
    vector.capacity = 0;
    vector.size = 0;
    auto* body = static_cast<FormData*>(request.httpBody);
    request.httpBody = nullptr;
    if (body) body->deref();
}

static void SetURLField(NKWebKit::WtfAbi::URL& field, StringImpl& impl, unsigned bits, unsigned pathEnd) {
    impl.refCount += 2;
    field.impl = &impl;
    field.bits = bits;
    field.userStart = 8;
    field.userEnd = 8;
    field.passwordEnd = 8;
    field.hostEnd = 24;
    field.pathAfterLastSlash = 25;
    field.pathEnd = pathEnd;
    field.queryEnd = pathEnd;
}

static void TestSetHTTPBody() {
    auto request = MakeRequest();
    auto* firstForm = CreateForm("abc", 3);
    auto* secondForm = CreateForm("def", 3);
    Require(firstForm->refCount == 1 && secondForm->refCount == 1);

    auto* body = firstForm;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    Require(request.httpBody == firstForm);
    Require(body == nullptr);
    Require(firstForm->refCount == 1);
    Require(!request.platformRequestBodyUpdated);
    Require(request.resourceRequestBodyUpdated);
    Require(request.platformRequestUpdated);

    body = secondForm;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    Require(request.httpBody == secondForm);
    Require(secondForm->refCount == 1);
    Require(body == nullptr);
    Require(guestAllocations.count(firstForm) == 0);

    secondForm->ref();
    body = secondForm;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    Require(request.httpBody == secondForm);
    Require(secondForm->refCount == 1);
    Require(body == nullptr);

    request.resourceRequestUpdated = false;
    secondForm->ref();
    body = secondForm;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    }));
    Require(request.httpBody == secondForm);
    Require(secondForm->refCount == 2);
    Require(body == secondForm);
    request.resourceRequestUpdated = true;

    body = nullptr;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    Require(request.httpBody == nullptr);
    Require(secondForm->refCount == 1);
    Require(body == nullptr);

    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(nullptr, &body);
    }));
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, nullptr);
    }));

    FreeForm(secondForm);
    Require(guestAllocations.empty());
}

static void TestClearPurposeAndReferrer() {
    auto request = MakeRequest();
    auto purpose = Make("high");
    auto referrer = Make("https://x.test/");
    String purposeText(&purpose);
    String referrerText(&referrer);
    purpose.refCount += 2;
    referrer.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Purpose, &purposeText);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Referer, &referrerText);
    request.platformRequestUpdated = true;

    _ZN7WebCore19ResourceRequestBase12clearPurposeEv(&request);
    Require(!request.platformRequestUpdated);
    Require(!_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Purpose));
    Require(purpose.refCount == 4);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase12clearPurposeEv(&request);
    Require(!request.platformRequestUpdated);

    _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(&request);
    Require(!_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Referer));
    Require(referrer.refCount == 4);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(&request);
    Require(!request.platformRequestUpdated);

    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Purpose, &purposeText);
    request.platformRequestUpdated = true;
    request.resourceRequestUpdated = false;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase12clearPurposeEv(&request); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(&request); }));
    Require(_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Purpose));
    Require(purpose.refCount == 6);
    request.resourceRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase12clearPurposeEv(nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(nullptr); }));

    Clear(request);
    Require(purpose.refCount == 4 && referrer.refCount == 4);
}

static void TestPartitionName() {
    StringImpl* shared = EmptyStringImpl();
    String null;
    String result = _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(&null);
    Require(result.impl == shared);
    Require(result.impl->length == 0);

    auto domain = Make("example.com");
    String domainText(&domain);
    domain.refCount += 2;
    String other = _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(&domainText);
    Require(other.impl == shared);
    Require(domain.refCount == 4);
    String third = _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(nullptr);
    Require(third.impl == shared);
    Require(third.impl->length == 0);
}

static void TestSetCachePartition() {
    StringImpl* shared = EmptyStringImpl();
    auto request = MakeRequest();
    auto partition = Make("example.com");
    String partitionText(&partition);
    partition.refCount += 2;
    _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(&request, &partitionText);
    Require(request.cachePartition.impl == nullptr);
    Require(partition.refCount == 4);
    Require(request.platformRequestUpdated);
    String nullPartition;
    _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(&request, &nullPartition);
    Require(request.platformRequestUpdated);
    _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(nullptr, &partitionText);
    Require(request.platformRequestUpdated);
    (void)shared;
}

static void TestSetHTTPHeaderFields() {
    auto request = MakeRequest();
    auto name = Make("content-TYPE");
    auto custom = Make("X-Custom");
    auto value = Make("v");
    auto junk = Make("j");
    String nameText(&name);
    String customText(&custom);
    String valueText(&value);
    String junkText(&junk);
    name.refCount += 2;
    custom.refCount += 2;
    value.refCount += 2;
    junk.refCount += 2;

    HTTPHeaderMap source{};
    HTTPHeaderMap empty{};
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&source, nameText, valueText);
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&source, customText, valueText);
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Headers(request), nameText, junkText);
    request.platformRequestUpdated = true;

    _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(&request, reinterpret_cast<NKWebKit::WebCoreAbi::HTTPHeaderMap*>(&source));
    Require(!request.platformRequestUpdated);
    Require(source.common.buffer == nullptr && source.common.size == 0 && source.common.capacity == 0);
    Require(source.uncommon.buffer == nullptr && source.uncommon.size == 0 && source.uncommon.capacity == 0);
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 1);
    Require(Headers(request).common.buffer[0].value.impl == &value);
    Require(Headers(request).uncommon.buffer[0].value.impl == &value);
    Require(junk.refCount == 4);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(&request, reinterpret_cast<NKWebKit::WebCoreAbi::HTTPHeaderMap*>(&empty));
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.buffer == nullptr && Headers(request).common.size == 0);
    Require(Headers(request).uncommon.buffer == nullptr && Headers(request).uncommon.size == 0);
    Require(value.refCount == 4);

    HTTPHeaderMap broken{};
    broken.common.size = 2;
    broken.common.capacity = 1;
    request.platformRequestUpdated = true;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(&request, reinterpret_cast<NKWebKit::WebCoreAbi::HTTPHeaderMap*>(&broken));
    }));
    Require(request.platformRequestUpdated);

    request.resourceRequestUpdated = false;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(&request, reinterpret_cast<NKWebKit::WebCoreAbi::HTTPHeaderMap*>(&source));
    }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(nullptr, reinterpret_cast<NKWebKit::WebCoreAbi::HTTPHeaderMap*>(&source));
    }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(&request, nullptr); }));
}

static void TestFallbackArray() {
    auto request = MakeRequest();
    auto one = Make("utf-8");
    auto two = Make("utf-16");
    auto three = Make("latin-1");
    String oneText(&one);
    String twoText(&two);
    String threeText(&three);
    one.refCount += 2;
    two.refCount += 2;
    three.refCount += 2;
    String null;

    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&request, &oneText, &twoText, &threeText);
    Require(!request.platformRequestUpdated);
    auto& vector = FallbackArray(request);
    Require(vector.size == 3 && vector.capacity == 3 && vector.buffer);
    Require(vector.buffer[0].impl == &one && vector.buffer[1].impl == &two && vector.buffer[2].impl == &three);
    Require(one.refCount == 6 && two.refCount == 6 && three.refCount == 6);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&request, &oneText, &null, &threeText);
    Require(vector.size == 2 && vector.capacity == 2);
    Require(vector.buffer[0].impl == &one && vector.buffer[1].impl == &three);
    Require(two.refCount == 4);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&request, &null, &null, &null);
    Require(vector.buffer == nullptr && vector.size == 0 && vector.capacity == 0);
    Require(one.refCount == 4 && three.refCount == 4);

    request.resourceRequestUpdated = false;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&request, &oneText, &null, &null);
    }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] {
        _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(nullptr, &oneText, &null, &null);
    }));
}

static void TestEqual() {
    auto left = MakeRequest();
    auto right = MakeRequest();
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    auto urlLeft = Make("https://a.test/path");
    auto urlRight = Make("https://a.test/path");
    auto partyLeft = Make("https://x.test/");
    auto partyRight = Make("https://x.test/");
    String urlLeftText(&urlLeft);
    String urlRightText(&urlRight);
    String partyLeftText(&partyLeft);
    String partyRightText(&partyRight);
    urlLeft.refCount += 2;
    urlRight.refCount += 2;
    partyLeft.refCount += 2;
    partyRight.refCount += 2;
    SetURLField(left.url, urlLeft, 3, 17);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    SetURLField(right.url, urlRight, 3, 17);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    right.cachePolicy = 1;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.cachePolicy = 0;
    right.timeoutInterval = 5;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.timeoutInterval = 0;

    SetURLField(left.firstPartyForCookies, partyLeft, 0, 0);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    SetURLField(right.firstPartyForCookies, partyRight, 0, 0);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    left.sameSiteDisposition = 1;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.sameSiteDisposition = 1;
    left.sameSiteDisposition = 2;
    right.sameSiteDisposition = 0;
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    left.sameSiteDisposition = 1;
    right.sameSiteDisposition = 1;
    left.isTopSite = true;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.isTopSite = true;

    auto method = Make("POST");
    String methodText(&method);
    method.refCount += 2;
    _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&left, &methodText);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&right, &methodText);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    left.allowCookies = false;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.allowCookies = false;
    left.priority = 3;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.priority = 3;
    left.requester = 2;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.requester = 2;
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    auto type = Make("text/html");
    String typeText(&type);
    type.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&left, HTTPHeaderName::ContentType, &typeText);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&right, HTTPHeaderName::ContentType, &typeText);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    auto plain = Make("text/plain");
    String plainText(&plain);
    plain.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&right, HTTPHeaderName::ContentType, &plainText);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&right, HTTPHeaderName::ContentType, &typeText);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    auto custom = Make("X-Custom");
    auto leftValue = Make("l");
    auto rightValue = Make("r");
    String customText(&custom);
    String leftValueText(&leftValue);
    String rightValueText(&rightValue);
    custom.refCount += 2;
    leftValue.refCount += 2;
    rightValue.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&left, &customText, &leftValueText);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&right, &customText, &rightValueText);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&right, &customText, &leftValueText);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));

    auto* bodyLeft = CreateForm("ab", 2);
    auto* bodyRight = CreateForm("ab", 2);
    left.httpBody = bodyLeft;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.httpBody = bodyRight;
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    _ZN7WebCore8FormData10appendDataEPKvm(bodyRight, "c", 1);
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.httpBody = bodyLeft;
    bodyRight->deref();
    Require(guestAllocations.count(bodyRight) == 0);
    Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    right.httpBody = nullptr;
    Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
    left.httpBody = nullptr;
    bodyLeft->deref();
    Require(guestAllocations.count(bodyLeft) == 0);

    {
        auto file = Make("file.bin");
        String fileText(&file);
        file.refCount += 2;
        auto* fileLeft = CreateEmptyForm();
        auto* fileRight = CreateEmptyForm();
        PushFile(fileLeft, fileText, 0, 5, true, 3);
        PushFile(fileRight, fileText, 0, 5, false, 3);
        left.httpBody = fileLeft;
        right.httpBody = fileRight;
        Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
        left.httpBody = nullptr;
        right.httpBody = nullptr;
        FreeForm(fileLeft);
        FreeForm(fileRight);

        auto* disengagedLeft = CreateEmptyForm();
        auto* disengagedRight = CreateEmptyForm();
        PushFile(disengagedLeft, fileText, 0, 5, false, 3);
        PushFile(disengagedRight, fileText, 0, 5, false, 7);
        left.httpBody = disengagedLeft;
        right.httpBody = disengagedRight;
        Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
        left.httpBody = nullptr;
        right.httpBody = nullptr;
        FreeForm(disengagedLeft);
        FreeForm(disengagedRight);

        auto* engagedLeft = CreateEmptyForm();
        auto* engagedRight = CreateEmptyForm();
        PushFile(engagedLeft, fileText, 0, 5, true, 3);
        PushFile(engagedRight, fileText, 0, 5, true, 4);
        left.httpBody = engagedLeft;
        right.httpBody = engagedRight;
        Require(!_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
        left.httpBody = nullptr;
        right.httpBody = nullptr;
        FreeForm(engagedLeft);
        FreeForm(engagedRight);
        Require(file.refCount == 4);
        Require(guestAllocations.empty());
    }

    {
        auto* valuelessLeft = CreateEmptyForm();
        auto* valuelessRight = CreateEmptyForm();
        PushValueless(valuelessLeft);
        PushValueless(valuelessRight);
        left.httpBody = valuelessLeft;
        right.httpBody = valuelessRight;
        Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right));
        Require(!_ZNK7WebCore19ResourceRequestBase9hasUploadEv(&left));
        left.httpBody = nullptr;
        right.httpBody = nullptr;
        FreeForm(valuelessLeft);
        FreeForm(valuelessRight);
        Require(guestAllocations.empty());
    }

    left.resourceRequestUpdated = false;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right); }));
    left.resourceRequestUpdated = true;
    left.resourceRequestBodyUpdated = false;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&left, &right); }));
    left.resourceRequestBodyUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(nullptr, &right); }));

    ReleaseRequest(left);
    ReleaseRequest(right);
    Require(guestAllocations.empty());
}

static void TestHTTPHeaderFieldString() {
    auto request = MakeRequest();
    auto known = Make("content-TYPE");
    auto custom = Make("X-Custom");
    auto value = Make("v");
    String knownText(&known);
    String customText(&custom);
    String valueText(&value);
    known.refCount += 2;
    custom.refCount += 2;
    value.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &knownText, &valueText);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &customText, &valueText);

    {
        String got = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, &knownText);
        Require(got.impl == &value && value.refCount == 10);
        String other = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, &customText);
        Require(other.impl == &value && value.refCount == 12);
        String missing = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, &knownText);
        Require(missing.impl == &value && value.refCount == 14);
    }
    Require(value.refCount == 8);

    auto absent = Make("X-Absent");
    String absentText(&absent);
    absent.refCount += 2;
    String absentValue = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, &absentText);
    Require(absentValue.impl == nullptr);

    request.resourceRequestUpdated = false;
    Require(Throws([&] { String got = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, &knownText); }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] { String got = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(nullptr, &knownText); }));
    Require(Throws([&] { String got = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(&request, nullptr); }));

    Clear(request);
    Require(known.refCount == 4 && custom.refCount == 4 && value.refCount == 4);
}

static void TestHTTPHeaderFieldsGetter() {
    auto request = MakeRequest();
    auto& fields = _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(&request);
    Require(static_cast<const void*>(&fields) == static_cast<const void*>(&request.httpHeaderFields));
    request.resourceRequestUpdated = false;
    Require(Throws([&] { auto& other = _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(&request); }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] { auto& other = _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(nullptr); }));
}

static void TestHTTPBodyHasUpload() {
    auto request = MakeRequest();
    Require(_ZNK7WebCore19ResourceRequestBase8httpBodyEv(&request).ptr == nullptr);
    Require(!_ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request));

    auto* form = CreateForm("x", 1);
    request.httpBody = form;
    auto body = _ZNK7WebCore19ResourceRequestBase8httpBodyEv(&request);
    Require(body.ptr == form);
    Require(form->refCount == 2);
    body.ptr->deref();
    Require(form->refCount == 1);
    Require(!_ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request));

    URL blobURL{};
    auto blobText = Make("https://blob.test/");
    blobURL.impl = &blobText;
    _ZN7WebCore8FormData10appendBlobERKN3WTF3URLE(form, &blobURL);
    Require(blobText.refCount == 4);
    Require(_ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request));

    auto file = Make("file.bin");
    String fileText(&file);
    file.refCount += 2;
    PushFile(form, fileText, 0, 5);
    Require(file.refCount == 6);
    Require(_ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request));

    request.resourceRequestBodyUpdated = false;
    Require(Throws([&] { auto other = _ZNK7WebCore19ResourceRequestBase8httpBodyEv(&request); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request); }));
    request.resourceRequestBodyUpdated = true;
    request.resourceRequestUpdated = false;
    Require(Throws([&] { auto other = _ZNK7WebCore19ResourceRequestBase8httpBodyEv(&request); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase9hasUploadEv(&request); }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] { auto other = _ZNK7WebCore19ResourceRequestBase8httpBodyEv(nullptr); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase9hasUploadEv(nullptr); }));

    request.httpBody = nullptr;
    form->deref();
    Require(file.refCount == 4 && blobText.refCount == 2);
    Require(guestAllocations.empty());
}

static void TestRemoveCredentials() {
    auto request = MakeRequest();
    auto plain = Make("https://a.test/path");
    String plainText(&plain);
    plain.refCount += 2;
    SetURLField(request.url, plain, 3, 19);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(&request);
    Require(request.platformRequestUpdated);
    Require(request.url.impl == &plain);
    Require(request.url.userStart == 8 && request.url.userEnd == 8 && request.url.passwordEnd == 8);

    auto credentialed = Make("https://user:pass@a.test/path");
    String credentialedText(&credentialed);
    credentialed.refCount += 2;
    NKWebKit::WtfAbi::Release(request.url.impl);
    SetURLField(request.url, credentialed, 3, 29);
    request.url.userEnd = 12;
    request.url.passwordEnd = 17;
    request.platformRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(&request); }));
    Require(request.platformRequestUpdated);
    Require(request.url.impl == &credentialed);
    Require(request.url.bits == 3);
    Require(request.url.userStart == 8 && request.url.userEnd == 12 && request.url.passwordEnd == 17);

    request.url.bits = 2;
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(&request);
    Require(!request.platformRequestUpdated);
    Require(request.url.impl == &credentialed);
    Require(request.url.userStart == 8 && request.url.userEnd == 12 && request.url.passwordEnd == 17);
    request.url.bits = 3;

    request.resourceRequestUpdated = false;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(&request); }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(nullptr); }));
    ReleaseRequest(request);
    Require(plain.refCount == 4);
    Require(credentialed.refCount == 4);
}

static void TestSetExistingHTTPReferrer() {
    auto request = MakeRequest();
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(&request);
    Require(request.platformRequestUpdated);

    auto referrer = Make("https://x.test/page");
    String referrerText(&referrer);
    referrer.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Referer, &referrerText);
    request.platformRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(&request); }));

    _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(&request);
    request.platformRequestUpdated = true;
    request.resourceRequestUpdated = false;
    _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(&request);
    Require(request.platformRequestUpdated);
    request.resourceRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Referer, &referrerText);
    request.platformRequestUpdated = true;
    request.resourceRequestUpdated = false;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(&request); }));
    request.resourceRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(nullptr); }));

    Clear(request);
    Require(referrer.refCount == 4);
}

static void TestIsolatedCopy() {
    {
        auto source = MakeRequest();
        auto url = Make("https://example.test/path");
        String urlText(&url);
        url.refCount += 2;
        SetURLField(source.url, url, 3, 25);

        auto method = Make("POST");
        String methodText(&method);
        method.refCount += 2;
        _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&source, &methodText);

        auto initiator = Make("worker-1");
        String initiatorText(&initiator);
        initiator.refCount += 2;
        source.initiatorIdentifier.impl = &initiator;
        initiator.refCount += 2;

        auto type = Make("text/html");
        String typeText(&type);
        type.refCount += 2;
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&source, HTTPHeaderName::ContentType, &typeText);

        auto customName = Make("X-Custom");
        auto customValue = Make("custom");
        String customNameText(&customName);
        String customValueText(&customValue);
        customName.refCount += 2;
        customValue.refCount += 2;
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&source, &customNameText, &customValueText);

        auto first = Make("utf-8");
        auto second = Make("utf-16");
        String firstText(&first);
        String secondText(&second);
        first.refCount += 2;
        second.refCount += 2;
        String null;
        _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&source, &firstText, &secondText, &null);

        auto partition = Make("example.test");
        String partitionText(&partition);
        partition.refCount += 2;
        source.cachePartition.impl = &partition;
        partition.refCount += 2;

        source.timeoutInterval = 30;
        source.cachePolicy = 2;
        source.priority = 3;
        source.requester = 5;
        source.isTopSite = true;
        source.allowCookies = false;
        source.isAppInitiated = false;
        source.sameSiteDisposition = 1;
        source.inspectorInitiatorNodeIdentifier = 7;

        auto copy = _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(&source);
        Require(copy.timeoutInterval == 30 && copy.cachePolicy == 2 && copy.priority == 3 && copy.requester == 5);
        Require(copy.isTopSite && !copy.allowCookies && !copy.isAppInitiated);
        Require(copy.sameSiteDisposition == 1);
        Require(copy.inspectorInitiatorNodeIdentifier.has_value() && *copy.inspectorInitiatorNodeIdentifier == 7);
        Require(copy.url.impl != &url);
        Require(TextOf(copy.url.impl) == "https://example.test/path");
        Require(copy.url.bits == 3 && copy.url.userStart == 8 && copy.url.userEnd == 8 && copy.url.passwordEnd == 8);
        Require(copy.url.hostEnd == 24 && copy.url.pathAfterLastSlash == 25 && copy.url.pathEnd == 25 && copy.url.queryEnd == 25);
        Require(copy.httpMethod.impl != &method && TextOf(copy.httpMethod.impl) == "POST");
        Require(copy.initiatorIdentifier.impl != &initiator && TextOf(copy.initiatorIdentifier.impl) == "worker-1");
        Require(copy.cachePartition.impl && copy.cachePartition.impl->length == 0);
        Require(copy.firstPartyForCookies.impl == nullptr && copy.httpBody == nullptr);
        auto& copyHeaders = *reinterpret_cast<HTTPHeaderMap*>(&copy.httpHeaderFields);
        Require(copyHeaders.common.size == 1 && copyHeaders.uncommon.size == 1);
        Require(copyHeaders.common.buffer[0].key == HTTPHeaderName::ContentType);
        Require(copyHeaders.common.buffer[0].value.impl != &type && TextOf(copyHeaders.common.buffer[0].value.impl) == "text/html");
        Require(copyHeaders.uncommon.buffer[0].key.impl != &customName && TextOf(copyHeaders.uncommon.buffer[0].key.impl) == "X-Custom");
        Require(copyHeaders.uncommon.buffer[0].value.impl != &customValue && TextOf(copyHeaders.uncommon.buffer[0].value.impl) == "custom");
        auto& copyArray = *reinterpret_cast<Vector<String>*>(&copy.responseContentDispositionEncodingFallbackArray);
        Require(copyArray.size == 2 && copyArray.capacity == 2);
        Require(copyArray.buffer[0].impl != &first && TextOf(copyArray.buffer[0].impl) == "utf-8");
        Require(copyArray.buffer[1].impl != &second && TextOf(copyArray.buffer[1].impl) == "utf-16");
        Require(copy.resourceRequestUpdated && !copy.platformRequestUpdated && copy.resourceRequestBodyUpdated && !copy.platformRequestBodyUpdated);
        Require(!copy.hiddenFromInspector);
        Require(source.url.impl == &url && source.initiatorIdentifier.impl == &initiator && source.cachePartition.impl == &partition);
        Require(Headers(source).common.size == 1 && Headers(source).uncommon.size == 1);
        Require(FallbackArray(source).size == 2);
        Require(url.refCount == 6 && method.refCount == 6 && initiator.refCount == 6 && partition.refCount == 6);
        ReleaseRequest(copy);
        ReleaseRequest(source);
    }

    {
        auto source = MakeRequest();
        source.httpBody = CreateForm("xy", 2);
        auto copy = _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(&source);
        auto* isolatedForm = static_cast<FormData*>(copy.httpBody);
        Require(isolatedForm && isolatedForm != source.httpBody);
        Require(isolatedForm->refCount == 1);
        Require(isolatedForm->elements.size == 1);
        Require(BodyBytes(isolatedForm->elements.buffer[0]) == "xy");
        Require(static_cast<FormData*>(source.httpBody)->refCount == 1);
        Require(!copy.platformRequestBodyUpdated && copy.resourceRequestBodyUpdated);
        ReleaseRequest(copy);
        ReleaseRequest(source);
        Require(guestAllocations.empty());
    }

    auto minimal = MakeRequest();
    auto get = Make("GET");
    String getText(&get);
    get.refCount += 2;
    _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&minimal, &getText);
    auto minimalCopy = _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(&minimal);
    Require(minimalCopy.httpMethod.impl != &get);
    Require(TextOf(minimalCopy.httpMethod.impl) == "GET");
    Require(minimalCopy.httpMethod.impl->refCount == 2);
    Require(minimalCopy.httpMethod.impl->hashAndFlags == 4);
    Require(minimalCopy.timeoutInterval == 0 && minimalCopy.cachePolicy == 0 && minimalCopy.priority == 1 && minimalCopy.requester == 0);
    Require(minimalCopy.allowCookies && minimalCopy.isAppInitiated && !minimalCopy.isTopSite);
    Require(minimalCopy.sameSiteDisposition == 0);
    Require(minimalCopy.initiatorIdentifier.impl == nullptr && minimalCopy.httpBody == nullptr);
    Require(!minimalCopy.inspectorInitiatorNodeIdentifier.has_value());
    Require(!minimalCopy.hiddenFromInspector);
    Require(minimalCopy.cachePartition.impl && minimalCopy.cachePartition.impl->length == 0);
    Require(minimalCopy.url.impl == nullptr && minimalCopy.firstPartyForCookies.impl == nullptr);
    auto& minimalHeaders = *reinterpret_cast<HTTPHeaderMap*>(&minimalCopy.httpHeaderFields);
    Require(minimalHeaders.common.size == 0 && minimalHeaders.uncommon.size == 0);
    auto& minimalArray = *reinterpret_cast<Vector<String>*>(&minimalCopy.responseContentDispositionEncodingFallbackArray);
    Require(minimalArray.size == 0 && minimalArray.buffer == nullptr);
    ReleaseRequest(minimalCopy);
    ReleaseRequest(minimal);

    auto gated = MakeRequest();
    gated.resourceRequestUpdated = false;
    Require(Throws([&] { auto other = _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(&gated); }));
    gated.resourceRequestUpdated = true;
    Require(Throws([&] { auto other = _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(nullptr); }));
    Require(guestAllocations.empty());
}

static void TestSetAsIsolatedCopy() {
    {
        auto destination = MakeRequest();
        auto junkUrl = Make("https://old.test/");
        String junkUrlText(&junkUrl);
        junkUrl.refCount += 2;
        SetURLField(destination.url, junkUrl, 3, 16);
        auto junkMethod = Make("DELETE");
        String junkMethodText(&junkMethod);
        junkMethod.refCount += 2;
        _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&destination, &junkMethodText);
        auto junkType = Make("junk/type");
        String junkTypeText(&junkType);
        junkType.refCount += 2;
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&destination, HTTPHeaderName::ContentType, &junkTypeText);
        destination.platformRequestUpdated = true;

        auto source = MakeRequest();
        auto url = Make("https://example.test/path");
        String urlText(&url);
        url.refCount += 2;
        SetURLField(source.url, url, 3, 25);
        auto method = Make("POST");
        String methodText(&method);
        method.refCount += 2;
        _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&source, &methodText);
        auto initiator = Make("worker-1");
        String initiatorText(&initiator);
        initiator.refCount += 2;
        source.initiatorIdentifier.impl = &initiator;
        initiator.refCount += 2;
        auto type = Make("text/html");
        String typeText(&type);
        type.refCount += 2;
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&source, HTTPHeaderName::ContentType, &typeText);
        auto customName = Make("X-Custom");
        auto customValue = Make("custom");
        String customNameText(&customName);
        String customValueText(&customValue);
        customName.refCount += 2;
        customValue.refCount += 2;
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&source, &customNameText, &customValueText);
        auto first = Make("utf-8");
        auto second = Make("utf-16");
        String firstText(&first);
        String secondText(&second);
        first.refCount += 2;
        second.refCount += 2;
        String null;
        _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&source, &firstText, &secondText, &null);
        auto partition = Make("example.test");
        String partitionText(&partition);
        partition.refCount += 2;
        source.cachePartition.impl = &partition;
        partition.refCount += 2;
        source.timeoutInterval = 30;
        source.cachePolicy = 2;
        source.priority = 3;
        source.requester = 5;
        source.isTopSite = true;
        source.allowCookies = false;
        source.isAppInitiated = false;
        source.sameSiteDisposition = 1;
        source.inspectorInitiatorNodeIdentifier = 7;
        source.httpBody = CreateForm("body", 4);

        _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(&destination, &source);
        Require(!destination.platformRequestUpdated);
        Require(destination.timeoutInterval == 30 && destination.cachePolicy == 2 && destination.priority == 3 && destination.requester == 5);
        Require(destination.isTopSite && !destination.allowCookies && !destination.isAppInitiated);
        Require(destination.sameSiteDisposition == 1);
        Require(destination.inspectorInitiatorNodeIdentifier.has_value() && *destination.inspectorInitiatorNodeIdentifier == 7);
        Require(destination.url.impl != &url && TextOf(destination.url.impl) == "https://example.test/path");
        Require(destination.url.bits == 3 && destination.url.pathEnd == 25);
        Require(destination.httpMethod.impl != &method && TextOf(destination.httpMethod.impl) == "POST");
        Require(destination.initiatorIdentifier.impl != &initiator && TextOf(destination.initiatorIdentifier.impl) == "worker-1");
        Require(destination.cachePartition.impl == nullptr);
        auto& destinationHeaders = *reinterpret_cast<HTTPHeaderMap*>(&destination.httpHeaderFields);
        Require(destinationHeaders.common.size == 1 && destinationHeaders.uncommon.size == 1);
        Require(destinationHeaders.common.buffer[0].value.impl != &type && TextOf(destinationHeaders.common.buffer[0].value.impl) == "text/html");
        auto& destinationArray = *reinterpret_cast<Vector<String>*>(&destination.responseContentDispositionEncodingFallbackArray);
        Require(destinationArray.size == 2 && TextOf(destinationArray.buffer[0].impl) == "utf-8" && TextOf(destinationArray.buffer[1].impl) == "utf-16");
        auto* destinationBody = static_cast<FormData*>(destination.httpBody);
        Require(destinationBody && destinationBody != source.httpBody);
        Require(destinationBody->refCount == 1);
        Require(destinationBody->elements.size == 1);
        Require(BodyBytes(destinationBody->elements.buffer[0]) == "body");
        Require(static_cast<FormData*>(source.httpBody)->refCount == 1);
        Require(junkUrl.refCount == 4 && junkMethod.refCount == 4 && junkType.refCount == 4);
        Require(_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(&destination, &source));

        Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(nullptr, &source); }));
        Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(&destination, nullptr); }));
        destination.resourceRequestUpdated = false;
        Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(&destination, &source); }));
        destination.resourceRequestUpdated = true;
        source.resourceRequestUpdated = false;
        Require(Throws([&] { _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(&destination, &source); }));
        source.resourceRequestUpdated = true;

        auto emptySource = MakeRequest();
        _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(&destination, &emptySource);
        auto& keptArray = *reinterpret_cast<Vector<String>*>(&destination.responseContentDispositionEncodingFallbackArray);
        Require(keptArray.size == 2 && keptArray.capacity == 2);
        Require(keptArray.buffer[0].impl != &first && TextOf(keptArray.buffer[0].impl) == "utf-8");
        Require(keptArray.buffer[1].impl != &second && TextOf(keptArray.buffer[1].impl) == "utf-16");
        Require(destination.inspectorInitiatorNodeIdentifier.has_value() && *destination.inspectorInitiatorNodeIdentifier == 7);
        Require(static_cast<FormData*>(destination.httpBody) == destinationBody);
        Require(destinationBody->refCount == 1);

        ReleaseRequest(destination);
        ReleaseRequest(emptySource);
        ReleaseRequest(source);
        Require(guestAllocations.empty());
    }
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestSetHTTPBody();
    TestClearPurposeAndReferrer();
    TestPartitionName();
    TestSetCachePartition();
    TestSetHTTPHeaderFields();
    TestFallbackArray();
    TestEqual();
    TestHTTPHeaderFieldString();
    TestHTTPHeaderFieldsGetter();
    TestHTTPBodyHasUpload();
    TestRemoveCredentials();
    TestSetExistingHTTPReferrer();
    TestIsolatedCopy();
    TestSetAsIsolatedCopy();
    Require(guestAllocations.empty());
    return 0;
}

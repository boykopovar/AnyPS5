#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
int remainingAllocations = -1;

void Require(bool condition, std::source_location location = std::source_location::current());

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (remainingAllocations == 0) return nullptr;
    if (remainingAllocations > 0) --remainingAllocations;
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (remainingAllocations == 0) return nullptr;
    if (remainingAllocations > 0) --remainingAllocations;
    void* result = std::calloc(count ? count : 1, size ? size : 1);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

void Require(bool condition, std::source_location location) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_set_issuer_name check failed at line %u\n", location.line());
    std::abort();
}

class TestName {
public:
    explicit TestName(const char* commonName = "example.com") {
        std::memcpy(commonName_.data(), commonName, std::strlen(commonName) + 1);
        values_[0] = Asn1String{2, 19, reinterpret_cast<unsigned char*>(const_cast<char*>("US")), 0};
        values_[1] = Asn1String{static_cast<std::int32_t>(std::strlen(commonName)), 12,
            reinterpret_cast<unsigned char*>(commonName_.data()), 0x10};
        entries_[0] = LibreSslX509NameEntry{const_cast<LibreSslAsn1Object*>(&LibreSslObjects_nid_no_patch[14]),
            &values_[0], 0, 0};
        entries_[1] = LibreSslX509NameEntry{const_cast<LibreSslAsn1Object*>(&LibreSslObjects_nid_no_patch[13]),
            &values_[1], 1, 0};
        entryPointers_[0] = reinterpret_cast<char*>(&entries_[0]);
        entryPointers_[1] = reinterpret_cast<char*>(&entries_[1]);
        stack_ = LibreSslStack{2, entryPointers_.data(), 0, 2, nullptr};
        name_ = LibreSslX509Name{&stack_, 0, nullptr, nullptr, 0};
    }

    LibreSslX509Name* Get() { return &name_; }

    void MutateCommonName(char value) { commonName_[0] = value; }

private:
    std::array<char, 64> commonName_{};
    std::array<Asn1String, 2> values_{};
    std::array<LibreSslX509NameEntry, 2> entries_{};
    std::array<char*, 2> entryPointers_{};
    LibreSslStack stack_{};
    LibreSslX509Name name_{};
};

std::string Text(const LibreSslX509Name* name, int nid) {
    std::array<char, 64> buffer{};
    const int length = X509_NAME_get_text_by_NID_nid_postfix(name, nid, buffer.data(), static_cast<int>(buffer.size()));
    Require(length >= 0);
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

void RejectsBadInput() {
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    TestName name;
    Require(X509_set_issuer_name_nid_postfix(nullptr, name.Get()) == 0);
    LibreSsl3Certificate empty{};
    Require(X509_set_issuer_name_nid_postfix(&empty, name.Get()) == 0);
    Require(X509_set_issuer_name_nid_postfix(&certificate, nullptr) == 0);
    Require(info.issuer == nullptr);
    Require(X509_NAME_dup_nid_no_patch(nullptr) == nullptr);
    X509_NAME_free_nid_no_patch(nullptr);
    Require(allocations.empty());
}

void DuplicatesAndReplaces() {
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    TestName first("first.example");
    TestName second("second.example");
    auto* firstName = X509_NAME_dup_nid_no_patch(first.Get());
    auto* secondName = X509_NAME_dup_nid_no_patch(second.Get());
    Require(firstName != nullptr && secondName != nullptr);

    Require(X509_set_issuer_name_nid_postfix(&certificate, firstName) == 1);
    auto* storedFirst = static_cast<LibreSslX509Name*>(info.issuer);
    Require(storedFirst != nullptr && storedFirst != firstName);
    Require(Text(storedFirst, 13) == "first.example");

    Require(X509_set_issuer_name_nid_postfix(&certificate, storedFirst) == 1);
    Require(info.issuer == storedFirst);

    Require(X509_set_issuer_name_nid_postfix(&certificate, secondName) == 1);
    auto* storedSecond = static_cast<LibreSslX509Name*>(info.issuer);
    Require(storedSecond != nullptr && storedSecond != storedFirst && storedSecond != secondName);
    Require(allocations.count(storedFirst) == 0);
    Require(Text(storedSecond, 13) == "second.example");

    X509_NAME_free_nid_no_patch(firstName);
    X509_NAME_free_nid_no_patch(secondName);
    Require(Text(storedSecond, 13) == "second.example");
    X509_NAME_free_nid_no_patch(storedSecond);
    Require(allocations.empty());
}

void DeepCopiesValuesAndObjects() {
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    TestName source("mutable.example");
    auto* callerName = X509_NAME_dup_nid_no_patch(source.Get());
    Require(callerName != nullptr);
    Require(X509_set_issuer_name_nid_postfix(&certificate, callerName) == 1);
    auto* stored = static_cast<LibreSslX509Name*>(info.issuer);
    Require(stored != nullptr && stored != callerName);

    auto* callerEntry = X509_NAME_get_entry_nid_postfix(callerName, 1);
    auto* storedEntry = X509_NAME_get_entry_nid_postfix(stored, 1);
    Require(callerEntry != nullptr && storedEntry != nullptr);
    Require(callerEntry->value != storedEntry->value);
    Require(callerEntry->object != storedEntry->object);
    auto* callerValue = static_cast<Asn1String*>(callerEntry->value);
    auto* storedValue = static_cast<Asn1String*>(storedEntry->value);
    Require(storedValue->data != nullptr && storedValue->data != callerValue->data);
    Require((storedValue->flags & 0x10) == 0);

    source.MutateCommonName('X');
    Require(Text(stored, 13) == "mutable.example");
    callerValue->data[0] = 'Y';
    Require(Text(stored, 13) == "mutable.example");
    X509_NAME_free_nid_no_patch(callerName);
    Require(Text(stored, 13) == "mutable.example");
    X509_NAME_free_nid_no_patch(stored);
    Require(allocations.empty());
}

void AllocationFailuresKeepThePreviousName() {
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    certificate.certInfo = &info;
    TestName first("kept.example");
    TestName second("new.example");
    auto* firstName = X509_NAME_dup_nid_no_patch(first.Get());
    Require(firstName != nullptr);
    Require(X509_set_issuer_name_nid_postfix(&certificate, firstName) == 1);
    auto* stored = static_cast<LibreSslX509Name*>(info.issuer);
    Require(stored != nullptr);
    const std::size_t before = allocations.size();

    remainingAllocations = 0;
    Require(X509_set_issuer_name_nid_postfix(&certificate, second.Get()) == 0);
    Require(info.issuer == stored);
    Require(allocations.size() == before);

    remainingAllocations = 5;
    Require(X509_set_issuer_name_nid_postfix(&certificate, second.Get()) == 0);
    Require(info.issuer == stored);
    Require(allocations.size() == before);

    remainingAllocations = -1;
    Require(X509_set_issuer_name_nid_postfix(&certificate, second.Get()) == 1);
    Require(info.issuer != stored);
    Require(Text(static_cast<LibreSslX509Name*>(info.issuer), 13) == "new.example");
    X509_NAME_free_nid_no_patch(firstName);
    X509_NAME_free_nid_no_patch(static_cast<LibreSslX509Name*>(info.issuer));
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 7> heapApi{reinterpret_cast<void*>(Allocate_nid_no_patch),
        reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Callocate_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch), reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    RejectsBadInput();
    DuplicatesAndReplaces();
    DeepCopiesValuesAndObjects();
    AllocationFailuresKeepThePreviousName();
    std::puts("libSceLibreSsl3 X509_set_issuer_name tests passed");
}

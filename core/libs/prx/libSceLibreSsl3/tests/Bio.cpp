#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
bool failAllocation = false;
int callbackCalls = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (std::exchange(failAllocation, false)) return nullptr;
    void* pointer = std::malloc(size);
    Require(pointer != nullptr && allocations.emplace(pointer, size).second);
    std::memset(pointer, 0xa5, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = Allocate_nid_no_patch(count * size);
    if (pointer) std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

struct Payload {
    std::array<char, 16> bytes{};
    int length = 0;
};

int APS5_VABI Write_nid_no_patch(LibreSsl3Bio* bio, const char* data, int length) {
    ++callbackCalls;
    auto* payload = static_cast<Payload*>(BIO_get_data_nid_postfix(bio));
    Require(length >= 0 && length <= static_cast<int>(payload->bytes.size()));
    std::memcpy(payload->bytes.data(), data, static_cast<std::size_t>(length));
    payload->length = length;
    return length;
}

int APS5_VABI Read_nid_no_patch(LibreSsl3Bio* bio, char* data, int capacity) {
    ++callbackCalls;
    auto* payload = static_cast<Payload*>(BIO_get_data_nid_postfix(bio));
    const int length = std::min(capacity, payload->length);
    std::memcpy(data, payload->bytes.data(), static_cast<std::size_t>(length));
    return length;
}

std::int64_t APS5_VABI Control_nid_no_patch(LibreSsl3Bio* bio, int command, std::int64_t value, void* argument) {
    ++callbackCalls;
    Require(command == 77 && value == 0x123456789abcdefLL && argument == BIO_get_data_nid_postfix(bio));
    return -0x123456789abcdefLL;
}

int APS5_VABI Create_nid_no_patch(LibreSsl3Bio* bio) {
    ++callbackCalls;
    BIO_set_init_nid_postfix(bio, 1);
    return 7;
}

int APS5_VABI Destroy_nid_no_patch(LibreSsl3Bio* bio) {
    ++callbackCalls;
    BIO_set_data_nid_postfix(bio, nullptr);
    BIO_set_init_nid_postfix(bio, 0);
    return 9;
}

void Methods() {
    char name[] = "guest BIO";
    auto* method = BIO_meth_new_nid_postfix(0x481, name);
    Require(method && allocations.at(method) == sizeof(LibreSsl3BioMethod));
    Require(method->type == 0x481 && method->name == name);
    Require(!method->write && !method->read && !method->puts && !method->gets && !method->control);
    Require(!method->create && !method->destroy && !method->callbackControl);
    LibreSsl3BioMethod expected;
    std::memcpy(&expected, method, sizeof(expected));
    expected.write = Write_nid_no_patch;
    Require(BIO_meth_set_write_nid_postfix(method, Write_nid_no_patch) == 1);
    Require(std::memcmp(method, &expected, sizeof(expected)) == 0);
    expected.read = Read_nid_no_patch;
    Require(BIO_meth_set_read_nid_postfix(method, Read_nid_no_patch) == 1);
    Require(std::memcmp(method, &expected, sizeof(expected)) == 0);
    expected.control = Control_nid_no_patch;
    Require(BIO_meth_set_ctrl_nid_postfix(method, Control_nid_no_patch) == 1);
    Require(std::memcmp(method, &expected, sizeof(expected)) == 0);
    expected.create = Create_nid_no_patch;
    Require(BIO_meth_set_create_nid_postfix(method, Create_nid_no_patch) == 1);
    Require(std::memcmp(method, &expected, sizeof(expected)) == 0);
    expected.destroy = Destroy_nid_no_patch;
    Require(BIO_meth_set_destroy_nid_postfix(method, Destroy_nid_no_patch) == 1);
    Require(std::memcmp(method, &expected, sizeof(expected)) == 0 && callbackCalls == 0);

    LibreSsl3Bio bio{};
    bio.method = method;
    Payload payload;
    BIO_set_data_nid_postfix(&bio, &payload);
    Require(method->create(&bio) == 7 && bio.initialized == 1);
    const char bytes[] = {'a', 0, static_cast<char>(0xff), 'b'};
    Require(method->write(&bio, bytes, sizeof(bytes)) == 4);
    std::array<char, 6> output{};
    Require(method->read(&bio, output.data(), 2) == 2 && std::memcmp(output.data(), bytes, 2) == 0);
    Require(method->read(&bio, output.data(), output.size()) == 4 && std::memcmp(output.data(), bytes, 4) == 0);
    Require(method->control(&bio, 77, 0x123456789abcdefLL, &payload) == -0x123456789abcdefLL);
    Require(method->destroy(&bio) == 9 && bio.initialized == 0 && BIO_get_data_nid_postfix(&bio) == nullptr);
    Require(callbackCalls == 6);
    Require(BIO_meth_set_write_nid_postfix(method, nullptr) == 1 && !method->write);
    Require(BIO_meth_set_read_nid_postfix(method, nullptr) == 1 && !method->read);
    Require(BIO_meth_set_ctrl_nid_postfix(method, nullptr) == 1 && !method->control);
    Require(BIO_meth_set_create_nid_postfix(method, nullptr) == 1 && !method->create);
    Require(BIO_meth_set_destroy_nid_postfix(method, nullptr) == 1 && !method->destroy);
    BIO_meth_free_nid_postfix(method);
    Require(name[0] == 'g' && allocations.empty() && callbackCalls == 6);
    BIO_meth_free_nid_postfix(nullptr);
    method = BIO_meth_new_nid_postfix(std::numeric_limits<int>::min(), nullptr);
    Require(method && method->type == std::numeric_limits<int>::min() && !method->name);
    BIO_meth_free_nid_postfix(method);
    failAllocation = true;
    ERR_clear_error_nid_postfix();
    Require(!BIO_meth_new_nid_postfix(1, name) && !failAllocation && allocations.empty());
    Require(ERR_get_error_nid_postfix() == 0);
}

void Fields() {
    struct Guarded {
        std::array<unsigned char, 16> before;
        LibreSsl3Bio bio;
        std::array<unsigned char, 16> after;
    } object;
    std::memset(&object, 0xa5, sizeof(object));
    Guarded expected;
    std::memcpy(&expected, &object, sizeof(object));
    int data = 42;
    BIO_set_data_nid_postfix(&object.bio, &data);
    expected.bio.data = &data;
    Require(BIO_get_data_nid_postfix(&object.bio) == &data && std::memcmp(&object, &expected, sizeof(object)) == 0);
    BIO_set_data_nid_postfix(&object.bio, nullptr);
    expected.bio.data = nullptr;
    Require(!BIO_get_data_nid_postfix(&object.bio) && std::memcmp(&object, &expected, sizeof(object)) == 0);
    for (int value : {0, 1, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        BIO_set_init_nid_postfix(&object.bio, value);
        expected.bio.initialized = value;
        Require(std::memcmp(&object, &expected, sizeof(object)) == 0);
        BIO_set_shutdown_nid_postfix(&object.bio, value);
        expected.bio.shutdown = value;
        Require(BIO_get_shutdown_nid_postfix(&object.bio) == value && std::memcmp(&object, &expected, sizeof(object)) == 0);
    }
    object.bio.flags = expected.bio.flags = 0x1234;
    for (int mask : {0, 0x80, -1, std::numeric_limits<int>::min()}) {
        BIO_set_flags_nid_postfix(&object.bio, mask);
        expected.bio.flags |= mask;
        Require(std::memcmp(&object, &expected, sizeof(object)) == 0);
        BIO_clear_flags_nid_postfix(&object.bio, mask);
        expected.bio.flags &= ~mask;
        Require(std::memcmp(&object, &expected, sizeof(object)) == 0);
    }
}

template<typename T>
void Throws(T action) {
    bool threw = false;
    try { action(); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

void NullObjects() {
    Throws([] { BIO_meth_set_write_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_meth_set_read_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_meth_set_ctrl_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_meth_set_create_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_meth_set_destroy_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_set_data_nid_postfix(nullptr, nullptr); });
    Throws([] { BIO_get_data_nid_postfix(nullptr); });
    Throws([] { BIO_set_init_nid_postfix(nullptr, 0); });
    Throws([] { BIO_set_shutdown_nid_postfix(nullptr, 0); });
    Throws([] { BIO_get_shutdown_nid_postfix(nullptr); });
    Throws([] { BIO_set_flags_nid_postfix(nullptr, 0); });
    Throws([] { BIO_clear_flags_nid_postfix(nullptr, 0); });
}

} // namespace

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Methods();
    Fields();
    NullObjects();
    Require(allocations.empty());
    std::puts("LibreSSL3 BIO tests passed");
}

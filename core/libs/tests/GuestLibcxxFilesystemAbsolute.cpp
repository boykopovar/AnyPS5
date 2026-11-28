#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr auto Absolute = _ZNSt3__14__fs10filesystem10__absoluteERKNS1_4pathEPNS_10error_codeE_nid_postfix;
constexpr char NotImplementedMessage[] =
    "_ZNSt3__14__fs10filesystem10__absoluteERKNS1_4pathEPNS_10error_codeE_nid_postfix not implemented";

using Descriptor = std::array<unsigned char, 24>;

struct ErrorCode {
    std::int32_t value;
    const void* category;
};

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fputs(failure, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

Descriptor Short(const std::string& text) {
    Descriptor descriptor{};
    descriptor[0] = static_cast<unsigned char>(text.size() << 1u);
    std::memcpy(descriptor.data() + 1, text.data(), text.size());
    return descriptor;
}

Descriptor Long(std::vector<char>& storage, const std::string& text) {
    storage.assign(((text.size() + 16) & ~std::size_t{15}), '\0');
    std::memcpy(storage.data(), text.data(), text.size());
    const std::uint64_t flags = storage.size() | 1u;
    const std::uint64_t size = text.size();
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(storage.data()));
    Descriptor descriptor{};
    std::memcpy(descriptor.data(), &flags, sizeof(flags));
    std::memcpy(descriptor.data() + 8, &size, sizeof(size));
    std::memcpy(descriptor.data() + 16, &address, sizeof(address));
    return descriptor;
}

Descriptor Call(const Descriptor& path) {
    Descriptor result{};
    result.fill(0xcc);
    Require(Absolute(&result, &path, nullptr) == &result, "__absolute did not return the result object");
    return result;
}

void RequireShort(const Descriptor& result, const std::string& expected, const char* failure) {
    Require(result[0] == expected.size() << 1u && std::memcmp(result.data() + 1, expected.data(), expected.size()) == 0 &&
                result[1 + expected.size()] == '\0',
            failure);
}

void RequireLong(const Descriptor& result, const std::string& expected, std::uint64_t flags, const char* failure) {
    std::uint64_t storedFlags = 0;
    std::uint64_t size = 0;
    char* data = nullptr;
    std::memcpy(&storedFlags, result.data(), sizeof(storedFlags));
    std::memcpy(&size, result.data() + 8, sizeof(size));
    std::memcpy(&data, result.data() + 16, sizeof(data));
    Require(storedFlags == flags && size == expected.size() && data != nullptr &&
                std::memcmp(data, expected.data(), expected.size()) == 0 && data[size] == '\0',
            failure);
    _ZdlPv_nid_postfix(data);
}

void RequireNotImplemented(const Descriptor& path, ErrorCode* error, const char* failure) {
    Descriptor result{};
    try {
        Absolute(&result, &path, error);
    } catch (const std::runtime_error& thrown) {
        Require(std::strcmp(thrown.what(), NotImplementedMessage) == 0, failure);
        return;
    }
    Require(false, failure);
}

void ChangeDirectory(const std::string& guest) {
    Require(chdir_nid_postfix(guest.c_str()) == 0, "guest chdir failed");
    std::array<char, 1024> current{};
    Require(getcwd_nid_postfix(current.data(), current.size()) != nullptr && guest == current.data(),
            "guest getcwd did not report the test directory");
}

}

int main() {
    const std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    std::string name;
    for (int index = 0; index < 100 && name.empty(); ++index) {
        const std::string candidate = std::string("f") + static_cast<char>('0' + index / 10) + static_cast<char>('0' + index % 10);
        std::error_code error;
        if (std::filesystem::create_directory(host / candidate, error)) name = candidate;
    }
    Require(!name.empty(), "no free test directory name");
    const auto root = "/" + name;
    const std::string exact(17, 'e');
    const std::string wide(30, 'w');
    Require(std::filesystem::create_directory(host / name / exact) && std::filesystem::create_directory(host / name / wide),
            "test subdirectories were not created");

    ChangeDirectory("/");
    RequireShort(Call(Short("/x/y")), "/x/y", "short absolute path was not copied unchanged");
    std::vector<char> storage;
    const std::string longAbsolute = "/" + std::string(39, 'L');
    RequireLong(Call(Long(storage, longAbsolute)), longAbsolute, 49, "long absolute path was not copied with libc++ capacity");
    const std::string minimalLong = "/" + std::string(22, 'M');
    RequireLong(Call(Long(storage, minimalLong)), minimalLong, 33, "23-byte absolute path did not get a 32-byte allocation");
    RequireShort(Call(Long(storage, "/q")), "/q", "short absolute path held in long form was not copied in short form");
    RequireShort(Call(Short("a")), "/a", "relative path from the root did not become /a");
    RequireShort(Call(Short("a/b")), "/a/b", "relative path from the root did not become /a/b");
    RequireShort(Call(Short("")), "/", "empty path from the root did not become /");

    ChangeDirectory(root);
    RequireShort(Call(Short("a/b")), root + "/a/b", "relative path was not appended to the current directory");
    RequireShort(Call(Short("")), root + "/", "empty path did not become the current directory with a separator");
    RequireShort(Call(Short("../c")), root + "/../c", "dot-dot components must be kept as given");
    const std::string twenty(20, 't');
    RequireLong(Call(Short(twenty)), root + "/" + twenty, 49, "append from a short directory did not grow to twice the short capacity");
    const std::string hundred(100, 'h');
    RequireLong(Call(Long(storage, hundred)), root + "/" + hundred, 113, "long relative path did not get the required capacity");

    const auto exactRoot = root + "/" + exact;
    Require(exactRoot.size() == 22, "test directory is not 22 characters long");
    ChangeDirectory(exactRoot);
    RequireLong(Call(Short("a")), exactRoot + "/a", 49, "separator after a full short directory did not grow the capacity");
    RequireLong(Call(Short("")), exactRoot + "/", 49, "empty path after a full short directory did not grow the capacity");

    const auto wideRoot = root + "/" + wide;
    ChangeDirectory(wideRoot);
    RequireLong(Call(Short("x")), wideRoot + "/x", 49, "append within a long directory's capacity changed it");
    RequireLong(Call(Short(twenty)), wideRoot + "/" + twenty, 97, "append past a long directory's capacity did not double it");

    ErrorCode code{7, &code};
    RequireNotImplemented(Short("/x"), &code, "a non-null error_code must throw");
    RequireNotImplemented(Short("x"), &code, "a non-null error_code must throw for a relative path");
    Require(code.value == 7 && code.category == &code, "the error_code was modified");

    ChangeDirectory(root);
    const std::string gone = "gone";
    Require(std::filesystem::create_directory(host / name / gone), "vanishing directory was not created");
    ChangeDirectory(root + "/" + gone);
    Require(std::filesystem::remove(host / name / gone), "vanishing directory was not removed");
    RequireNotImplemented(Short("x"), nullptr, "a failing getcwd must throw");
    RequireShort(Call(Short("/x")), "/x", "absolute path must not need the current directory");

    ChangeDirectory("/");
    std::filesystem::remove_all(host / name);
    std::puts("libc++ filesystem absolute checks passed");
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

struct GuestDescriptorSet {
    std::array<std::uint64_t, 1024 / 64> words{};

    void Add(int descriptor) {
        words[descriptor / 64] |= (std::uint64_t{1} << (descriptor % 64));
    }

    bool Has(int descriptor) const {
        return ((words[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
    }
};

extern "C" {
int APS5_VABI select_nid_postfix(int, void*, void*, void*, const void*);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI close_nid_postfix(int);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestDescriptorLimit = 1024;

int main() {
    int descriptors[2]{-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    const int reader = descriptors[0];
    const int writer = descriptors[1];
    Require(reader >= 0 && writer >= 0 && reader != writer);

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        GuestTimeval limit{0, 200000};
        const auto start = std::chrono::steady_clock::now();
        Require(select_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit) == 0);
        Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(150));
        Require(!read.Has(reader));
    }

    {
        const char byte = 's';
        Require(write_nid_postfix(writer, &byte, 1) == 1);
        GuestDescriptorSet read{};
        GuestDescriptorSet except{};
        read.Add(reader);
        except.Add(reader);
        GuestTimeval limit{0, 200000};
        Require(select_nid_postfix(reader + 1, read.words.data(), nullptr, except.words.data(), &limit) == 1);
        Require(read.Has(reader));
        Require(!except.Has(reader));
        char buffer[1]{};
        Require(read_nid_postfix(reader, buffer, sizeof(buffer)) == 1);
        Require(buffer[0] == byte);
    }

    {
        GuestDescriptorSet write{};
        write.Add(writer);
        GuestTimeval limit{0, 200000};
        Require(select_nid_postfix(writer + 1, nullptr, write.words.data(), nullptr, &limit) == 1);
        Require(write.Has(writer));
    }

    {
        const char byte = 'e';
        Require(write_nid_postfix(writer, &byte, 1) == 1);
        GuestDescriptorSet read{};
        GuestDescriptorSet write{};
        read.Add(reader);
        write.Add(writer);
        const int highest = reader > writer ? reader : writer;
        GuestTimeval limit{0, 200000};
        Require(select_nid_postfix(highest + 1, read.words.data(), write.words.data(), nullptr, &limit) == 2);
        Require(read.Has(reader) && write.Has(writer));
    }

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        GuestTimeval limit{0, 100000};
        Require(select_nid_postfix(reader, read.words.data(), nullptr, nullptr, &limit) == 0);
        Require(read.Has(reader));
    }

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        GuestTimeval limit{0, 50000};
        Require(select_nid_postfix(GuestDescriptorLimit + 1, read.words.data(), nullptr, nullptr, &limit) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }

    Require(close_nid_postfix(reader) == 0);
    Require(close_nid_postfix(writer) == 0);

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        GuestTimeval limit{0, 50000};
        Require(select_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit) == -1);
        Require(*__error_nid_postfix() == GuestEbadf);
    }
}

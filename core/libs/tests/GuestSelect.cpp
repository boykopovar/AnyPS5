#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

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

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestDescriptorLimit = 1024;
constexpr std::int16_t EvfiltUser = -11;
constexpr std::uint16_t EvAdd = 0x0001;
constexpr std::uint16_t EvClear = 0x0020;
constexpr std::uint32_t NoteTrigger = 0x01000000;

static int SelectAll(int descriptor, GuestDescriptorSet& read, GuestDescriptorSet& write, GuestDescriptorSet& except, const GuestTimeval& limit) {
    read = {};
    write = {};
    except = {};
    read.Add(descriptor);
    write.Add(descriptor);
    except.Add(descriptor);
    return select_nid_postfix(descriptor + 1, read.words.data(), write.words.data(), except.words.data(), &limit);
}

static void SocketsShutForReading() {
    constexpr int UnixDomain = 1, Stream = 1;
    int pair[2];
    Require(socketpair_nid_postfix(UnixDomain, Stream, 0, pair) == 0);
    Require(shutdown_nid_postfix(pair[0], 0) == 0);
    GuestDescriptorSet read{};
    read.Add(pair[0]);
    const GuestTimeval zero{0, 0};
    Require(select_nid_postfix(pair[0] + 1, read.words.data(), nullptr, nullptr, &zero) == 1 && read.Has(pair[0]));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(socketpair_nid_postfix(UnixDomain, Stream, 0, pair) == 0);
    read = {};
    read.Add(pair[0]);
    const GuestTimeval limit{5, 0};
    const auto start = std::chrono::steady_clock::now();
    std::thread shutter([&pair] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(shutdown_nid_postfix(pair[0], 0) == 0);
    });
    const int ready = select_nid_postfix(pair[0] + 1, read.words.data(), nullptr, nullptr, &limit);
    const auto waited = std::chrono::steady_clock::now() - start;
    shutter.join();
    Require(ready == 1 && read.Has(pair[0]));
    Require(waited >= std::chrono::milliseconds(40) && waited < std::chrono::seconds(4));
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
}

static KernelEvent UserEvent(std::uint16_t flags, std::uint32_t fflags) {
    KernelEvent change{};
    change.ident = 7;
    change.filter = EvfiltUser;
    change.flags = flags;
    change.fflags = fflags;
    return change;
}

static void StandardDescriptorFile(int descriptor) {
    const int saved = dup_nid_postfix(descriptor);
    const std::string path = "anyps5-select-input-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int file = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
    Require(file >= 0);
    Require(dup2_nid_postfix(file, descriptor) == descriptor);
    if (file != descriptor) Require(close_nid_postfix(file) == 0);
    GuestDescriptorSet read{};
    GuestDescriptorSet write{};
    GuestDescriptorSet except{};
    const GuestTimeval zero{0, 0};
    const int ready = SelectAll(descriptor, read, write, except, zero);
    if (ready != 2) std::fprintf(stderr, "select on a regular file at descriptor %d returned %d instead of 2\n", descriptor, ready);
    Require(ready == 2);
    Require(read.Has(descriptor) && write.Has(descriptor) && !except.Has(descriptor));
    char byte = 0;
    Require(read_nid_postfix(descriptor, &byte, 1) == 0);
    if (saved >= 0) {
        Require(dup2_nid_postfix(saved, descriptor) == descriptor);
        Require(close_nid_postfix(saved) == 0);
    } else Require(close_nid_postfix(descriptor) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
}

static void Kqueues() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0 && kq < GuestDescriptorLimit);
    GuestDescriptorSet read{};
    GuestDescriptorSet write{};
    GuestDescriptorSet except{};
    const GuestTimeval none{0, 0};
    Require(SelectAll(kq, read, write, except, none) == 0);
    Require(!read.Has(kq) && !write.Has(kq) && !except.Has(kq));
    const auto added = UserEvent(EvAdd | EvClear, 0);
    Require(kevent_nid_postfix(kq, &added, 1, nullptr, 0, nullptr) == 0);
    Require(SelectAll(kq, read, write, except, none) == 0);
    std::thread trigger([kq] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto triggered = UserEvent(0, NoteTrigger);
        Require(kevent_nid_postfix(kq, &triggered, 1, nullptr, 0, nullptr) == 0);
    });
    const GuestTimeval second{1, 0};
    Require(SelectAll(kq, read, write, except, second) == 1);
    trigger.join();
    Require(read.Has(kq) && !write.Has(kq) && !except.Has(kq));
    Require(SelectAll(kq, read, write, except, none) == 1 && read.Has(kq));
    KernelEvent events[1]{};
    const KernelTimespec zero{0, 0};
    Require(kevent_nid_postfix(kq, nullptr, 0, events, 1, &zero) == 1);
    Require(SelectAll(kq, read, write, except, none) == 0);
    Require(close_nid_postfix(kq) == 0);
}

int main() {
    for (const int descriptor : {0, 1, 2}) StandardDescriptorFile(descriptor);
    Kqueues();
    SocketsShutForReading();

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
        const GuestTimeval limit{0, 1000};
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) {
            GuestDescriptorSet read{};
            read.Add(reader);
            Require(select_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit) == 0);
            Require(!read.Has(reader));
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        Require(elapsed >= std::chrono::milliseconds(20) && elapsed < std::chrono::milliseconds(200));
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
        const std::string path = "anyps5-select-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const int file = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
        const int random = open_nid_postfix("/dev/urandom", 0, 0);
        Require(file >= 0 && random >= 0);
        GuestDescriptorSet read{};
        GuestDescriptorSet write{};
        GuestDescriptorSet except{};
        for (const int descriptor : {file, random}) {
            read.Add(descriptor);
            write.Add(descriptor);
            except.Add(descriptor);
        }
        read.Add(reader);
        const int highest = file > random ? (file > reader ? file : reader) : (random > reader ? random : reader);
        const GuestTimeval zero{0, 0};
        Require(select_nid_postfix(highest + 1, read.words.data(), write.words.data(), except.words.data(), &zero) == 3);
        Require(read.Has(file) && write.Has(file) && !except.Has(file));
        Require(read.Has(random) && !write.Has(random) && !except.Has(random));
        GuestDescriptorSet randomWrite{};
        GuestDescriptorSet randomExcept{};
        randomWrite.Add(random);
        randomExcept.Add(random);
        Require(select_nid_postfix(random + 1, nullptr, randomWrite.words.data(), randomExcept.words.data(), &zero) == 2);
        Require(randomWrite.Has(random) && randomExcept.Has(random));
        Require(!read.Has(reader));
        GuestDescriptorSet onlyExcept{};
        onlyExcept.Add(file);
        Require(select_nid_postfix(file + 1, nullptr, nullptr, onlyExcept.words.data(), &zero) == 0 && !onlyExcept.Has(file));
        Require(close_nid_postfix(random) == 0 && close_nid_postfix(file) == 0 && unlink_nid_postfix(path.c_str()) == 0);
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

    Require(reader < 63);

    {
        GuestDescriptorSet read{};
        GuestDescriptorSet except{};
        read.Add(reader);
        read.Add(63);
        read.Add(64);
        except.Add(63);
        GuestTimeval limit{0, 100000};
        Require(select_nid_postfix(reader, read.words.data(), nullptr, except.words.data(), &limit) == 0);
        Require(!read.Has(reader) && !read.Has(63) && read.Has(64) && !except.Has(63));
    }

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        const GuestTimeval zero{0, 0};
        Require(select_nid_postfix(0, read.words.data(), nullptr, nullptr, &zero) == 0);
        Require(read.Has(reader));
    }

    {
        const char byte = 'n';
        Require(write_nid_postfix(writer, &byte, 1) == 1);
        GuestDescriptorSet read{};
        read.Add(reader);
        read.Add(63);
        read.Add(64);
        GuestTimeval limit{0, 200000};
        Require(select_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit) == 1);
        Require(read.Has(reader) && !read.Has(63) && read.Has(64));
        char buffer[8]{};
        const auto count = read_nid_postfix(reader, buffer, sizeof(buffer));
        Require(count >= 1 && buffer[count - 1] == byte);
    }

    {
        GuestDescriptorSet read{};
        read.Add(reader);
        GuestTimeval limit{0, 50000};
        Require(select_nid_postfix(GuestDescriptorLimit + 1, read.words.data(), nullptr, nullptr, &limit) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }

    {
        int pipe[2]{-1, -1};
        Require(pipe_nid_postfix(pipe) == 0);
        const int receiver = socket_nid_postfix(2, 2, 0);
        const int sender = socket_nid_postfix(2, 2, 0);
        Require(receiver >= 0 && receiver < GuestDescriptorLimit && sender >= 0 && sender < GuestDescriptorLimit);
        std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
        std::uint32_t size = address.size();
        Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0);
        Require(connect_nid_postfix(sender, address.data(), address.size()) == 0);
        const int highest = receiver > pipe[0] ? receiver : pipe[0];
        GuestDescriptorSet idle{};
        idle.Add(receiver);
        idle.Add(pipe[0]);
        GuestTimeval none{0, 0};
        Require(select_nid_postfix(highest + 1, idle.words.data(), nullptr, nullptr, &none) == 0);
        Require(!idle.Has(receiver) && !idle.Has(pipe[0]));
        const char byte = 'm';
        Require(send_nid_postfix(sender, &byte, 1, 0) == 1);
        Require(write_nid_postfix(pipe[1], &byte, 1) == 1);
        GuestDescriptorSet read{};
        GuestTimeval limit{2, 0};
        const auto start = std::chrono::steady_clock::now();
        int ready = 0;
        while (ready < 2 && std::chrono::steady_clock::now() - start < std::chrono::seconds(2)) {
            read = {};
            read.Add(receiver);
            read.Add(pipe[0]);
            ready = select_nid_postfix(highest + 1, read.words.data(), nullptr, nullptr, &limit);
        }
        Require(ready == 2 && read.Has(receiver) && read.Has(pipe[0]));
        char buffer[1]{};
        Require(recv_nid_postfix(receiver, buffer, sizeof(buffer), 0) == 1 && buffer[0] == byte);
        Require(read_nid_postfix(pipe[0], buffer, sizeof(buffer)) == 1 && buffer[0] == byte);
        for (const int descriptor : {receiver, sender, pipe[0], pipe[1]}) Require(close_nid_postfix(descriptor) == 0);
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

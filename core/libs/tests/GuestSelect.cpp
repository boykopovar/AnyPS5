#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstdint>
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

extern "C" {
int APS5_VABI select_nid_postfix(int, void*, void*, void*, const void*);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI read_nid_postfix(int, void*, std::uint64_t);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI unlink_nid_postfix(const char*);
int APS5_VABI kqueue_nid_postfix();
int APS5_VABI kevent_nid_postfix(int, const KernelEvent*, int, KernelEvent*, int, const KernelTimespec*);
int* APS5_VABI __error_nid_postfix();
}

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

static KernelEvent UserEvent(std::uint16_t flags, std::uint32_t fflags) {
    KernelEvent change{};
    change.ident = 7;
    change.filter = EvfiltUser;
    change.flags = flags;
    change.fflags = fflags;
    return change;
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
    Kqueues();

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

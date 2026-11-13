#include "SceTypes.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <string>
#include <thread>

namespace {

constexpr int Enoent = 2;
constexpr int Ebadf = 9;
constexpr int Efault = 14;
constexpr int Eexist = 17;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int Emsgsize = 40;
constexpr int Etimedout = 60;
constexpr int Enametoolong = 63;
constexpr int ORdonly = 0x0000;
constexpr int OWronly = 0x0001;
constexpr int ORdwr = 0x0002;
constexpr int ONonblock = 0x0004;
constexpr int OCreat = 0x0200;
constexpr int OExcl = 0x0800;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::string Name(const char* tag) {
    return std::string("/anyps5-mq-") + tag + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

GuestTimespec FromNow(std::chrono::milliseconds delay) {
    const auto when = std::chrono::system_clock::now() + delay;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(when.time_since_epoch()).count();
    return {nanoseconds / 1000000000, nanoseconds % 1000000000};
}

std::string Receive(int queue, unsigned* priority = nullptr) {
    char buffer[16]{};
    const auto length = kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), priority, nullptr);
    Require(length >= 0);
    return std::string(buffer, static_cast<std::size_t>(length));
}

void OpenRules() {
    const auto name = Name("open");
    const GuestMqAttr zero{0, 0, 16, 0, {}};
    const GuestMqAttr many{0, 101, 16, 0, {}};
    const GuestMqAttr empty{0, 2, 0, 0, {}};
    const GuestMqAttr huge{0, 2, 16385, 0, {}};
    Require(kmq_open_nid_postfix(name.c_str(), 3 | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &zero) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &many) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &empty) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &huge) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("relative", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("/", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    Require(kmq_open_nid_postfix("/a/b", ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Einval);
    const std::string longName = "/" + std::string(300, 'q');
    Require(kmq_open_nid_postfix(longName.c_str(), ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Enametoolong);
    Require(kmq_open_nid_postfix(nullptr, ORdwr | OCreat, 0600, nullptr) == -1 && Error() == Efault);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr, 0600, nullptr) == -1 && Error() == Enoent);
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, nullptr);
    Require(queue >= 0);
    GuestMqAttr attributes{};
    Require(kmq_setattr_nid_postfix(queue, nullptr, &attributes) == 0);
    Require(attributes.maxmsg == 10 && attributes.msgsize == 1024 && attributes.curmsgs == 0 && attributes.flags == 0);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat | OExcl, 0600, nullptr) == -1 && Error() == Eexist);
    Require(close_nid_postfix(queue) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == -1 && Error() == Enoent);
    Require(kmq_unlink_nid_postfix("bad") == -1 && Error() == Einval);
}

void Messages() {
    const auto name = Name("messages");
    const GuestMqAttr small{0, 3, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &small);
    Require(queue >= 0);
    Require(kmq_timedsend_nid_postfix(queue, "x", 1, 64, nullptr) == -1 && Error() == Einval);
    Require(kmq_timedsend_nid_postfix(queue, "0123456789abcdefg", 17, 1, nullptr) == -1 && Error() == Emsgsize);
    Require(kmq_timedsend_nid_postfix(queue, "low", 3, 1, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "high", 4, 5, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "low2", 4, 1, nullptr) == 0);
    GuestMqAttr attributes{};
    const GuestMqAttr nonblocking{ONonblock, 0, 0, 0, {}};
    const GuestMqAttr bad{0x1, 0, 0, 0, {}};
    Require(kmq_setattr_nid_postfix(queue, &bad, nullptr) == -1 && Error() == Einval);
    Require(kmq_setattr_nid_postfix(queue, &nonblocking, &attributes) == 0 && attributes.flags == 0 && attributes.curmsgs == 3);
    Require(kmq_setattr_nid_postfix(queue, nullptr, &attributes) == 0 && attributes.flags == ONonblock);
    Require(kmq_timedsend_nid_postfix(queue, "more", 4, 1, nullptr) == -1 && Error() == Eagain);
    char tiny[8]{};
    Require(kmq_timedreceive_nid_postfix(queue, tiny, sizeof(tiny), nullptr, nullptr) == -1 && Error() == Emsgsize);
    unsigned priority = 99;
    Require(Receive(queue, &priority) == "high" && priority == 5);
    Require(Receive(queue, &priority) == "low" && priority == 1);
    Require(Receive(queue, &priority) == "low2" && priority == 1);
    char buffer[16]{};
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Eagain);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void Waiting() {
    const auto name = Name("waiting");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    char buffer[16]{};
    const GuestTimespec invalid{0, 1000000000};
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &invalid) == -1 && Error() == Einval);
    const auto deadline = FromNow(std::chrono::milliseconds(150));
    const auto start = std::chrono::steady_clock::now();
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &deadline) == -1 && Error() == Etimedout);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(100));
    Require(kmq_timedsend_nid_postfix(queue, "a", 1, 0, &invalid) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "b", 1, 0, &invalid) == -1 && Error() == Einval);
    Require(kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &invalid) == 1 && buffer[0] == 'a');
    std::atomic<std::int64_t> received{-2};
    std::thread reader([&] {
        char local[16]{};
        received = kmq_timedreceive_nid_postfix(queue, local, sizeof(local), nullptr, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(received == -2);
    Require(kmq_timedsend_nid_postfix(queue, "wake", 4, 3, nullptr) == 0);
    reader.join();
    Require(received == 4);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void Access() {
    const auto name = Name("access");
    const int both = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, nullptr);
    const int reader = kmq_open_nid_postfix(name.c_str(), ORdonly, 0600, nullptr);
    const int writer = kmq_open_nid_postfix(name.c_str(), OWronly | ONonblock, 0600, nullptr);
    Require(both >= 0 && reader >= 0 && writer >= 0);
    char buffer[1024]{};
    Require(kmq_timedsend_nid_postfix(reader, "x", 1, 0, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_timedreceive_nid_postfix(writer, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_timedsend_nid_postfix(writer, "shared", 6, 0, nullptr) == 0);
    Require(kmq_unlink_nid_postfix(name.c_str()) == 0);
    Require(kmq_open_nid_postfix(name.c_str(), ORdwr, 0600, nullptr) == -1 && Error() == Enoent);
    Require(kmq_timedreceive_nid_postfix(reader, buffer, sizeof(buffer), nullptr, nullptr) == 6);
    Require(close_nid_postfix(both) == 0 && close_nid_postfix(reader) == 0);
    Require(kmq_timedreceive_nid_postfix(reader, buffer, sizeof(buffer), nullptr, nullptr) == -1 && Error() == Ebadf);
    const auto path = "anyps5-mq-stale-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    { std::ofstream stream(path); stream << "x"; }
    const int plain = open_nid_postfix(path.c_str(), 0, 0);
    Require(plain >= 0);
    Require(kmq_timedsend_nid_postfix(plain, "x", 1, 0, nullptr) == -1 && Error() == Ebadf);
    Require(kmq_setattr_nid_postfix(plain, nullptr, nullptr) == -1 && Error() == Ebadf);
    Require(close_nid_postfix(plain) == 0 && close_nid_postfix(writer) == 0);
    std::filesystem::remove(path);
}

void NullBuffer() {
    const auto name = Name("nullbuffer");
    const GuestMqAttr small{0, 3, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &small);
    Require(queue >= 0);
    Require(kmq_timedsend_nid_postfix(queue, "keep", 4, 2, nullptr) == 0);
    Require(kmq_timedreceive_nid_postfix(queue, nullptr, 16, nullptr, nullptr) == -1 && Error() == Efault);
    unsigned priority = 0;
    Require(Receive(queue, &priority) == "keep" && priority == 2);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void FarDeadline() {
    const auto name = Name("fardeadline");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    const GuestTimespec forever{INT64_MAX, 0};
    std::atomic<std::int64_t> received{-2};
    std::thread reader([&] {
        char local[16]{};
        received = kmq_timedreceive_nid_postfix(queue, local, sizeof(local), nullptr, &forever);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(received == -2);
    Require(kmq_timedsend_nid_postfix(queue, "late", 4, 0, nullptr) == 0);
    reader.join();
    Require(received == 4);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

void ConcurrentSetattr() {
    const auto name = Name("setattr");
    const GuestMqAttr one{0, 1, 16, 0, {}};
    const int queue = kmq_open_nid_postfix(name.c_str(), ORdwr | OCreat, 0600, &one);
    Require(queue >= 0);
    std::atomic<bool> stop{false};
    std::thread toggler([&] {
        const GuestMqAttr on{ONonblock, 0, 0, 0, {}};
        const GuestMqAttr off{0, 0, 0, 0, {}};
        while (!stop) {
            kmq_setattr_nid_postfix(queue, &on, nullptr);
            kmq_setattr_nid_postfix(queue, &off, nullptr);
        }
    });
    char buffer[16]{};
    for (int i = 0; i < 200; ++i) {
        const auto deadline = FromNow(std::chrono::milliseconds(1));
        const auto result = kmq_timedreceive_nid_postfix(queue, buffer, sizeof(buffer), nullptr, &deadline);
        Require(result == -1 && (Error() == Etimedout || Error() == Eagain));
    }
    stop = true;
    toggler.join();
    const GuestMqAttr off{0, 0, 0, 0, {}};
    Require(kmq_setattr_nid_postfix(queue, &off, nullptr) == 0);
    Require(kmq_timedsend_nid_postfix(queue, "ok", 2, 0, nullptr) == 0);
    unsigned priority = 7;
    Require(Receive(queue, &priority) == "ok" && priority == 0);
    Require(close_nid_postfix(queue) == 0 && kmq_unlink_nid_postfix(name.c_str()) == 0);
}

}

int main() {
    for (int iter = 0; iter < 2; ++iter) OpenRules();
    Messages();
    Waiting();
    Access();
    NullBuffer();
    FarDeadline();
    ConcurrentSetattr();
    std::puts("message queues passed");
    return 0;
}

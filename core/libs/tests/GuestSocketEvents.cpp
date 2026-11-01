#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int EVFILT_READ = -1;
static constexpr int EVFILT_WRITE = -2;
static constexpr std::uint16_t EV_EOF = 0x8000;

static void Require(bool value) { if (!value) std::abort(); }

static bool Rejects(int (APS5_VABI *add)(KernelEqueue, int, std::size_t, void*), KernelEqueue eq, int fd, std::size_t size) {
    try {
        add(eq, fd, size, nullptr);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void PipeReadEvents() {
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "pipe") == SCE_OK);
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    const KernelUseconds shortTimeout = 20000;
    const KernelUseconds timeout = 1000000;
    int marker = 0;
    int count = 0;
    std::array<KernelEvent, 4> events{};

    Require(sceKernelAddReadEvent(eq, pipe[0], 64, &marker) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &shortTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(count == 0);
    Require(write_nid_postfix(pipe[1], "abc", 3) == 3);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].ident == static_cast<std::uintptr_t>(pipe[0]) && events[0].filter == EVFILT_READ);
    Require(events[0].data == 3 && events[0].udata == &marker && (events[0].flags & EV_EOF) == 0);
    char buffer[4]{};
    Require(read_nid_postfix(pipe[0], buffer, sizeof(buffer)) == 3);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &shortTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);

    const auto start = std::chrono::steady_clock::now();
    std::thread writer([&pipe] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(write_nid_postfix(pipe[1], "d", 1) == 1);
    });
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, nullptr) == SCE_OK);
    writer.join();
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    Require(count == 1 && events[0].data == 1);

    Require(close_nid_postfix(pipe[1]) == 0);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].data == 1 && (events[0].flags & EV_EOF) != 0);
    Require(read_nid_postfix(pipe[0], buffer, sizeof(buffer)) == 1);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].data == 0 && (events[0].flags & EV_EOF) != 0);
    Require(sceKernelDeleteReadEvent(eq, pipe[0]) == SCE_OK);
    Require(sceKernelDeleteReadEvent(eq, pipe[0]) == SCE_KERNEL_ERROR_ENOENT);

    int oneWay[2];
    Require(pipe_nid_postfix(oneWay) == 0);
    Require(Rejects(sceKernelAddWriteEvent, eq, oneWay[1], 1));
    Require(close_nid_postfix(oneWay[0]) == 0 && close_nid_postfix(oneWay[1]) == 0);
    Require(close_nid_postfix(pipe[0]) == 0);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

static void RegularFileEvents() {
    const std::string path = "anyps5-equeue-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int file = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
    Require(file >= 0);
    Require(write_nid_postfix(file, "0123456789", 10) == 10);
    Require(lseek_nid_postfix(file, 2, 0) == 2);
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "file") == SCE_OK);
    const KernelUseconds shortTimeout = 20000;
    const KernelUseconds timeout = 1000000;
    int marker = 0;
    int count = 0;
    std::array<KernelEvent, 4> events{};
    Require(sceKernelAddReadEvent(eq, file, 1, &marker) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].ident == static_cast<std::uintptr_t>(file) && events[0].filter == EVFILT_READ);
    Require(events[0].data == 8 && events[0].udata == &marker);
    Require(lseek_nid_postfix(file, 0, 2) == 10);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &shortTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    const int appender = open_nid_postfix(path.c_str(), 0x0002, 0);
    Require(appender >= 0);
    std::thread writer([appender] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(lseek_nid_postfix(appender, 0, 2) == 10);
        Require(write_nid_postfix(appender, "abc", 3) == 3);
    });
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, nullptr) == SCE_OK);
    writer.join();
    Require(count == 1 && events[0].data == 3);
    Require(sceKernelDeleteReadEvent(eq, file) == SCE_OK);
    Require(sceKernelAddWriteEvent(eq, file, 0, nullptr) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].filter == EVFILT_WRITE && events[0].data == 0);
    Require(sceKernelDeleteWriteEvent(eq, file) == SCE_OK);
    Require(Rejects(sceKernelAddReadEvent, eq, file, 64));
    Require(Rejects(sceKernelAddWriteEvent, eq, file, 64));
    Require(sceKernelAddReadEvent(eq, file, 0, nullptr) == SCE_OK);
    Require(close_nid_postfix(file) == 0);
    Require(sceKernelDeleteReadEvent(eq, file) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceKernelAddReadEvent(eq, file, 0, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(close_nid_postfix(appender) == 0);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
    Require(std::filesystem::remove(path));
}

static void CloseWhileWaitingOnFile() {
    const std::string path = "anyps5-equeue-close-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int created = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
    Require(created >= 0);
    Require(close_nid_postfix(created) == 0);
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "close") == SCE_OK);
    std::atomic<bool> stop{false};
    std::thread waiter([eq, &stop] {
        const KernelUseconds timeout = 1000;
        std::array<KernelEvent, 4> events{};
        int count = 0;
        while (!stop.load()) {
            Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
        }
    });
    for (int i = 0; i < 2000; ++i) {
        const int file = open_nid_postfix(path.c_str(), 0x0002, 0);
        Require(file >= 0);
        Require(sceKernelAddReadEvent(eq, file, 0, nullptr) == SCE_OK);
        Require(close_nid_postfix(file) == 0);
    }
    stop.store(true);
    waiter.join();
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
    Require(std::filesystem::remove(path));
}

static void Dup2OntoWatchedFile() {
    const std::string path = "anyps5-equeue-dup2-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const int created = open_nid_postfix(path.c_str(), 0x0002 | 0x0200, 0644);
    Require(created >= 0);
    Require(close_nid_postfix(created) == 0);
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "dup2") == SCE_OK);
    std::atomic<bool> stop{false};
    std::thread waiter([eq, &stop] {
        const KernelUseconds timeout = 1000;
        std::array<KernelEvent, 4> events{};
        int count = 0;
        while (!stop.load()) {
            Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
        }
    });
    for (int i = 0; i < 2000; ++i) {
        const int watched = open_nid_postfix(path.c_str(), 0x0002, 0);
        const int replacement = open_nid_postfix(path.c_str(), 0x0002, 0);
        Require(watched >= 0 && replacement >= 0);
        Require(sceKernelAddReadEvent(eq, watched, 0, nullptr) == SCE_OK);
        Require(dup2_nid_postfix(replacement, watched) == watched);
        Require(close_nid_postfix(replacement) == 0);
        Require(close_nid_postfix(watched) == 0);
    }
    stop.store(true);
    waiter.join();
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
    Require(std::filesystem::remove(path));
}

int main() {
    PipeReadEvents();
    RegularFileEvents();
    CloseWhileWaitingOnFile();
    Dup2OntoWatchedFile();
    const int listener = socket_nid_postfix(2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "sockets") == SCE_OK);
    const KernelUseconds shortTimeout = 20000;
    const KernelUseconds timeout = 1000000;
    int marker = 0;
    int count = 0;
    std::array<KernelEvent, 4> events{};

    Require(sceKernelAddReadEvent(eq, listener, 1, &marker) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &shortTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(count == 0);

    const int client = socket_nid_postfix(2, 1, 0);
    Require(client >= 0);
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1);
    Require(events[0].ident == static_cast<std::uintptr_t>(listener));
    Require(events[0].filter == EVFILT_READ);
    Require(events[0].udata == &marker);
    Require(events[0].data == 1);
    const int second = socket_nid_postfix(2, 1, 0);
    Require(second >= 0 && connect_nid_postfix(second, address.data(), address.size()) == 0);
#ifdef __linux__
    const auto queued = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    do {
        Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
        Require(count == 1 && events[0].ident == static_cast<std::uintptr_t>(listener));
    } while (events[0].data != 2 && std::chrono::steady_clock::now() < queued);
    Require(events[0].data == 2);
#endif
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const int acceptedSecond = accept_nid_postfix(listener, nullptr, nullptr);
    Require(acceptedSecond >= 0);
    Require(close_nid_postfix(acceptedSecond) == 0 && close_nid_postfix(second) == 0);
    Require(sceKernelDeleteReadEvent(eq, listener) == SCE_OK);
    Require(sceKernelDeleteReadEvent(eq, listener) == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelAddWriteEvent(eq, client, 0, nullptr) == SCE_OK);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].filter == EVFILT_WRITE && events[0].data > 0);
    Require(sceKernelDeleteWriteEvent(eq, client) == SCE_OK);

    Require(sceKernelAddReadEvent(eq, accepted, 1, nullptr) == SCE_OK);
    const char message[] = "ready";
    Require(send_nid_postfix(client, message, sizeof(message), 0) == sizeof(message));
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].ident == static_cast<std::uintptr_t>(accepted));
    Require(events[0].data == sizeof(message) && (events[0].flags & EV_EOF) == 0);
    char received[sizeof(message)]{};
    Require(recv_nid_postfix(accepted, received, sizeof(received), 0) == sizeof(received));
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &shortTimeout) == SCE_KERNEL_ERROR_ETIMEDOUT);

    Require(close_nid_postfix(client) == 0);
    Require(sceKernelWaitEqueue(eq, events.data(), events.size(), &count, &timeout) == SCE_OK);
    Require(count == 1 && events[0].data == 0 && (events[0].flags & EV_EOF) != 0);

    KernelEqueue blocking = 0;
    Require(sceKernelCreateEqueue(&blocking, "blocking") == SCE_OK);
    const int writer = socket_nid_postfix(2, 1, 0);
    Require(writer >= 0 && connect_nid_postfix(writer, address.data(), address.size()) == 0);
    const int reader = accept_nid_postfix(listener, nullptr, nullptr);
    Require(reader >= 0);
    Require(sceKernelAddReadEvent(blocking, reader, 1, nullptr) == SCE_OK);
    const auto start = std::chrono::steady_clock::now();
    std::thread sender([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Require(send_nid_postfix(writer, message, sizeof(message), 0) == sizeof(message));
    });
    Require(sceKernelWaitEqueue(blocking, events.data(), events.size(), &count, nullptr) == SCE_OK);
    sender.join();
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    Require(count == 1 && events[0].ident == static_cast<std::uintptr_t>(reader) && events[0].data == sizeof(message));
    Require(recv_nid_postfix(reader, received, sizeof(received), 0) == sizeof(received));

    std::thread waiter([&] {
        std::array<KernelEvent, 4> woken{};
        int wokenCount = 0;
        Require(sceKernelWaitEqueue(blocking, woken.data(), woken.size(), &wokenCount, nullptr) == SCE_OK);
        Require(wokenCount == 1 && woken[0].filter == EVFILT_WRITE && woken[0].ident == static_cast<std::uintptr_t>(writer));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelAddWriteEvent(blocking, writer, 0, nullptr) == SCE_OK);
    waiter.join();
    Require(sceKernelDeleteWriteEvent(blocking, writer) == SCE_OK);

    Require(close_nid_postfix(reader) == 0);
    Require(sceKernelDeleteReadEvent(blocking, reader) == SCE_KERNEL_ERROR_ENOENT);
    Require(close_nid_postfix(writer) == 0);
    Require(sceKernelDeleteEqueue(blocking) == SCE_OK);

    Require(sceKernelAddReadEvent(eq, client, 1, nullptr) == SCE_KERNEL_ERROR_EBADF);
    Require(Rejects(sceKernelAddReadEvent, eq, 0, 1));
    Require(Rejects(sceKernelAddWriteEvent, eq, accepted, 64));

    Require(close_nid_postfix(accepted) == 0);
    Require(close_nid_postfix(listener) == 0);
    Require(sceKernelDeleteEqueue(eq) == SCE_OK);
}

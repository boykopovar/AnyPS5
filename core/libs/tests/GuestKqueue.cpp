#include "SceTypes.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#endif

extern "C" {
int APS5_VABI kqueue_nid_postfix();
int APS5_VABI kevent_nid_postfix(int, const KernelEvent*, int, KernelEvent*, int, const KernelTimespec*);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI dup_nid_postfix(int);
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI mkdir_nid_postfix(const char*, unsigned short);
int APS5_VABI rmdir_nid_postfix(const char*);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int APS5_VABI ftruncate_nid_postfix(int, std::int64_t);
int APS5_VABI unlink_nid_postfix(const char*);
int APS5_VABI sceKernelRename(const char*, const char*);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelAddFileEvent(KernelEqueue eq, int fd, int watch, void* udata);
int APS5_VABI sceKernelDeleteFileEvent(KernelEqueue eq, int fd);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int GuestEnoent = 2;
static constexpr int GuestEbadf = 9;
static constexpr int GuestEinval = 22;
static constexpr int Unix = 1;
static constexpr int Stream = 1;
static constexpr int FirstSocket = 0x10000000;
static constexpr std::int16_t EvfiltRead = -1;
static constexpr std::int16_t EvfiltWrite = -2;
static constexpr std::int16_t EvfiltVnode = -4;
static constexpr std::int16_t EvfiltProc = -5;
static constexpr std::int16_t EvfiltTimer = -7;
static constexpr std::int16_t EvfiltUser = -11;
static constexpr std::uint16_t EvAdd = 0x0001;
static constexpr std::uint16_t EvDelete = 0x0002;
static constexpr std::uint16_t EvEnable = 0x0004;
static constexpr std::uint16_t EvDisable = 0x0008;
static constexpr std::uint16_t EvOneshot = 0x0010;
static constexpr std::uint16_t EvClear = 0x0020;
static constexpr std::uint16_t EvReceipt = 0x0040;
static constexpr std::uint16_t EvDispatch = 0x0080;
static constexpr std::uint16_t EvForceoneshot = 0x0100;
static constexpr std::uint16_t EvError = 0x4000;
static constexpr std::uint32_t NoteTrigger = 0x01000000;
static constexpr std::uint32_t NoteFfcopy = 0xc0000000;
static constexpr std::uint32_t NoteFfor = 0x80000000;
static constexpr std::uint32_t NoteUseconds = 0x0004;
static constexpr std::uint32_t NoteDelete = 0x0001;
static constexpr std::uint32_t NoteWrite = 0x0002;
static constexpr std::uint32_t NoteExtend = 0x0004;
static constexpr std::uint32_t NoteAttrib = 0x0008;
static constexpr std::uint32_t NoteLink = 0x0010;
static constexpr std::uint32_t NoteRename = 0x0020;
static constexpr std::uint32_t NoteRevoke = 0x0040;
static constexpr int ReadWrite = 0x0002;
static constexpr int Create = 0x0200;
static constexpr int Directory = 0x00020000;
static constexpr int SceOk = 0;
static constexpr int SceEnoent = static_cast<int>(0x80020002);
static constexpr int SceEbadf = static_cast<int>(0x80020009);
static constexpr int SceEinval = static_cast<int>(0x80020016);

static void Require(bool value) { if (!value) std::abort(); }

static KernelEvent Change(std::uintptr_t ident, std::int16_t filter, std::uint16_t flags, std::uint32_t fflags = 0, std::intptr_t data = 0, void* udata = nullptr) {
    KernelEvent change{};
    change.ident = ident;
    change.filter = filter;
    change.flags = flags;
    change.fflags = fflags;
    change.data = data;
    change.udata = udata;
    return change;
}

static int Apply(int kq, const KernelEvent& change) {
    return kevent_nid_postfix(kq, &change, 1, nullptr, 0, nullptr);
}

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

static int Poll(int kq, KernelEvent* events, int count) {
    const KernelTimespec zero{0, 0};
    return kevent_nid_postfix(kq, nullptr, 0, events, count, &zero);
}

static int Wait(int kq, KernelEvent* events, int count) {
    const KernelTimespec second{1, 0};
    return kevent_nid_postfix(kq, nullptr, 0, events, count, &second);
}

template <typename TCall> static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

template <typename TCall> static std::string ThrownMessage(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

static void CreateAndClose() {
    const int kq = kqueue_nid_postfix();
    Require(kq >= 0 && kq < FirstSocket);
    const int other = kqueue_nid_postfix();
    Require(other >= 0 && other != kq);
    KernelEvent event{};
    Require(Poll(kq, &event, 1) == 0);
    Require(Throws([kq] { dup_nid_postfix(kq); }));
    Require(close_nid_postfix(kq) == 0);
    RequireFailure(Poll(kq, &event, 1), GuestEbadf);
    Require(close_nid_postfix(other) == 0);
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    RequireFailure(Poll(pipe[0], &event, 1), GuestEbadf);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
}

static void Timers() {
    const int kq = kqueue_nid_postfix();
    int marker = 0;
    KernelEvent events[4]{};
    Require(Apply(kq, Change(1, EvfiltTimer, EvAdd, 0, 10, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == 1 && events[0].filter == EvfiltTimer && events[0].data >= 1 && events[0].udata == &marker);
    Require((events[0].flags & EvClear) != 0);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == 1);
    Require(Apply(kq, Change(1, EvfiltTimer, EvDelete)) == 0);
    RequireFailure(Apply(kq, Change(1, EvfiltTimer, EvDelete)), GuestEnoent);

    Require(Apply(kq, Change(2, EvfiltTimer, EvAdd | EvOneshot, NoteUseconds, 0)) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == 2 && events[0].data == 1);
    Require(Poll(kq, events, 4) == 0);
    RequireFailure(Apply(kq, Change(2, EvfiltTimer, EvDelete)), GuestEnoent);

    RequireFailure(Apply(kq, Change(3, EvfiltTimer, EvAdd, 0, -1)), GuestEinval);
    RequireFailure(Apply(kq, Change(3, EvfiltTimer, EvAdd, 0x10, 5)), GuestEinval);
    RequireFailure(Apply(kq, Change(3, EvfiltTimer, EvAdd, NoteUseconds | 0x1, 5)), GuestEinval);
    Require(Apply(kq, Change(4, EvfiltTimer, EvAdd, 0, 100000)) == 0);
    Require(Throws([kq] { Apply(kq, Change(4, EvfiltTimer, EvAdd, 0, 5)); }));
    Require(close_nid_postfix(kq) == 0);
}

static void UserEvents() {
    const int kq = kqueue_nid_postfix();
    int marker = 0;
    KernelEvent events[4]{};
    Require(Apply(kq, Change(5, EvfiltUser, EvAdd | EvClear, 0, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);
    std::thread trigger([kq, &marker] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(Apply(kq, Change(5, EvfiltUser, 0, NoteTrigger | NoteFfcopy | 0x123, 77, &marker)) == 0);
    });
    Require(kevent_nid_postfix(kq, nullptr, 0, events, 4, nullptr) == 1);
    trigger.join();
    Require(events[0].ident == 5 && events[0].filter == EvfiltUser && events[0].fflags == 0x123 && events[0].data == 77);
    Require(events[0].udata == &marker && events[0].flags == EvClear);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(5, EvfiltUser, 0, NoteTrigger | NoteFfor | 0x400, 78)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].fflags == 0x523 && events[0].data == 78);

    Require(Apply(kq, Change(6, EvfiltUser, EvAdd)) == 0);
    Require(Apply(kq, Change(6, EvfiltUser, 0, NoteTrigger)) == 0);
    for (int i = 0; i < 2; ++i) {
        Require(Poll(kq, events, 4) == 1 && events[0].ident == 6);
    }
    Require(Apply(kq, Change(6, EvfiltUser, EvClear)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(6, EvfiltUser, EvDelete)) == 0);

    KernelEvent receipt[2]{};
    const KernelEvent change = Change(7, EvfiltUser, EvAdd | EvReceipt, 0x55, 9, &marker);
    Require(kevent_nid_postfix(kq, &change, 1, receipt, 2, nullptr) == 1);
    Require(receipt[0].ident == 7 && receipt[0].flags == EvError && receipt[0].data == 0 && receipt[0].fflags == 0);
    Require(Apply(kq, Change(7, EvfiltUser, 0, NoteTrigger)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].fflags == 0x55 && events[0].data == 0 && events[0].flags == EvReceipt);
    Require(close_nid_postfix(kq) == 0);
}

static void SocketPair() {
    const int kq = kqueue_nid_postfix();
    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    int marker = 0;
    KernelEvent events[4]{};
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvAdd, 0, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(send_nid_postfix(pair[1], "abc", 3, 0) == 3);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == static_cast<std::uintptr_t>(pair[0]) && events[0].filter == EvfiltRead && events[0].data == 3 && events[0].udata == &marker);
    Require(Poll(kq, events, 4) == 1);
    char buffer[4]{};
    Require(recv_nid_postfix(pair[0], buffer, sizeof(buffer), 0) == 3);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[1]), EvfiltWrite, EvAdd | EvOneshot)) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].filter == EvfiltWrite);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pair[1]), EvfiltWrite, EvDelete)), GuestEnoent);
    Require(Throws([kq, &pair] { Apply(kq, Change(static_cast<std::uintptr_t>(pair[1]), EvfiltRead, EvAdd | EvClear)); }));
    Require(close_nid_postfix(pair[0]) == 0);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvDelete)), GuestEbadf);
    Require(close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(kq) == 0);
}

static void Errors() {
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const KernelEvent changes[3] = {
        Change(FirstSocket + 4000, EvfiltRead, EvAdd),
        Change(8, EvfiltUser, EvDelete),
        Change(9, EvfiltUser, EvAdd | EvEnable | EvDisable),
    };
    Require(kevent_nid_postfix(kq, changes, 3, events, 4, nullptr) == 3);
    Require(events[0].flags == EvError && events[0].data == GuestEbadf && events[0].ident == FirstSocket + 4000);
    Require(events[1].flags == EvError && events[1].data == GuestEnoent);
    Require(events[2].flags == EvError && events[2].data == GuestEinval);
    RequireFailure(kevent_nid_postfix(kq, changes, 3, events, 1, nullptr), GuestEnoent);
    Require(events[0].flags == EvError && events[0].data == GuestEbadf);
    RequireFailure(kevent_nid_postfix(kq, changes, 3, nullptr, 0, nullptr), GuestEbadf);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(kq), EvfiltRead, EvAdd)), GuestEinval);
    RequireFailure(Apply(kq, Change(1, 1, EvAdd)), GuestEinval);
    Require(Throws([kq] { Apply(kq, Change(1, EvfiltProc, EvAdd)); }));
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    Require(Throws([kq, &pipe] { Apply(kq, Change(static_cast<std::uintptr_t>(pipe[0]), EvfiltRead, EvAdd)); }));
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);
    const KernelEvent skipped = Change(1, 0, EvAdd);
    const KernelTimespec zero{0, 0};
    Require(kevent_nid_postfix(kq, &skipped, 1, events, 4, &zero) == 0);
    const KernelTimespec invalid{0, 1000000000};
    RequireFailure(kevent_nid_postfix(kq, nullptr, 0, events, 4, &invalid), GuestEinval);
    const KernelTimespec negative{-1, 0};
    RequireFailure(kevent_nid_postfix(kq, nullptr, 0, events, 4, &negative), GuestEinval);
    Require(close_nid_postfix(kq) == 0);
}

static std::string TemporaryName(const char* stem) {
    return std::string("anyps5-kqueue-") + stem + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

static void FileWatch() {
    const auto path = TemporaryName("file");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const int kq = kqueue_nid_postfix();
    int marker = 0;
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(file);
    Require(Apply(kq, Change(ident, EvfiltVnode, 0x25, NoteDelete | NoteWrite | NoteExtend | NoteLink | NoteRename, 0, &marker)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(write_nid_postfix(file, "abc", 3) == 3);
    Require(Wait(kq, events, 4) == 1);
    Require(events[0].ident == ident && events[0].filter == EvfiltVnode && events[0].flags == EvClear && events[0].udata == &marker);
    Require(events[0].fflags == (NoteWrite | NoteExtend) && events[0].data == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(ftruncate_nid_postfix(file, 0) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd, NoteAttrib)) == 0);
    Require(write_nid_postfix(file, "abc", 3) == 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(ftruncate_nid_postfix(file, 0) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].fflags == NoteAttrib && events[0].udata == nullptr);
    Require(Throws([kq, ident] { Apply(kq, Change(ident, EvfiltVnode, EvAdd, NoteRevoke)); }));
    Require(Apply(kq, Change(ident, EvfiltVnode, EvDelete)) == 0);
    RequireFailure(Apply(kq, Change(ident, EvfiltVnode, EvDelete)), GuestEnoent);

    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "file") == SceOk);
    Require(sceKernelAddFileEvent(eq, file, static_cast<int>(NoteWrite), &marker) == SceOk);
    Require(write_nid_postfix(file, "defg", 4) == 4);
    int count = 0;
    const KernelUseconds second = 1000000;
    Require(sceKernelWaitEqueue(eq, events, 4, &count, &second) == SceOk && count == 1);
    Require(events[0].ident == ident && events[0].filter == EvfiltVnode && events[0].fflags == (NoteWrite | NoteExtend) && events[0].udata == &marker);
    Require(sceKernelDeleteFileEvent(eq, file) == SceOk);
    Require(sceKernelDeleteFileEvent(eq, file) == SceEnoent);
    Require(sceKernelAddFileEvent(eq, FirstSocket + 4000, static_cast<int>(NoteWrite), nullptr) == SceEbadf);
    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(sceKernelAddFileEvent(eq, pair[0], static_cast<int>(NoteWrite), nullptr) == SceEinval);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(kq), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    int pipe[2];
    Require(pipe_nid_postfix(pipe) == 0);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pipe[0]), EvfiltVnode, EvAdd, NoteWrite)), GuestEinval);
    Require(close_nid_postfix(pipe[0]) == 0 && close_nid_postfix(pipe[1]) == 0);

    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd, NoteWrite)) == 0);
    Require(sceKernelAddFileEvent(eq, file, static_cast<int>(NoteWrite), nullptr) == SceOk);
    Require(close_nid_postfix(file) == 0);
    RequireFailure(Apply(kq, Change(ident, EvfiltVnode, EvDelete)), GuestEbadf);
    Require(sceKernelDeleteFileEvent(eq, file) == SceEbadf);
    Require(sceKernelDeleteEqueue(eq) == SceOk);
    Require(close_nid_postfix(kq) == 0);
    Require(std::filesystem::remove(path));
}

#ifndef _WIN32
static void RenameAndDeleteWatchedFile() {
    const auto path = TemporaryName("moved");
    const auto renamed = path + "-renamed";
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(file);
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteDelete | NoteRename)) == 0);
    Require(sceKernelRename(path.c_str(), renamed.c_str()) == SceOk);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == NoteRename);
    Require(unlink_nid_postfix(renamed.c_str()) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == NoteDelete);
    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(kq) == 0);
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static std::uint32_t NotesThrough(int kq, std::uintptr_t ident, std::uint32_t last) {
    std::uint32_t notes = 0;
    KernelEvent events[4]{};
    while ((notes & last) == 0) {
        Require(Wait(kq, events, 4) == 1 && events[0].ident == ident);
        notes |= events[0].fflags;
    }
    return notes;
}

static void MoveDirectoryEntries() {
    const auto path = TemporaryName("moves");
    const auto outside = path + "-outside";
    Require(mkdir_nid_postfix(path.c_str(), 0755) == 0 && mkdir_nid_postfix(outside.c_str(), 0755) == 0);
    const int child = open_nid_postfix((path + "/first.bin").c_str(), ReadWrite | Create, 0644);
    Require(child >= 0 && close_nid_postfix(child) == 0);
    const int directory = open_nid_postfix(path.c_str(), Directory, 0);
    Require(directory >= 0);
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(directory);
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite | NoteExtend | NoteLink)) == 0);
    for (int i = 0; i < 64; ++i) {
        Require(sceKernelRename((path + "/first.bin").c_str(), (path + "/second.bin").c_str()) == SceOk);
        Require(sceKernelRename((path + "/second.bin").c_str(), (path + "/first.bin").c_str()) == SceOk);
    }
    Require(mkdir_nid_postfix((path + "/sub").c_str(), 0755) == 0);
    Require(NotesThrough(kq, ident, NoteLink) == (NoteWrite | NoteLink));
    Require(sceKernelRename((path + "/first.bin").c_str(), (outside + "/first.bin").c_str()) == SceOk);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == (NoteWrite | NoteExtend));
    Require(sceKernelRename((outside + "/first.bin").c_str(), (path + "/first.bin").c_str()) == SceOk);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == (NoteWrite | NoteExtend));
    Require(sceKernelRename((path + "/sub").c_str(), (outside + "/sub").c_str()) == SceOk);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == (NoteWrite | NoteExtend | NoteLink));
    Require(close_nid_postfix(directory) == 0);
    Require(close_nid_postfix(kq) == 0);
    Require(std::filesystem::remove_all(path) == 2 && std::filesystem::remove_all(outside) == 2);
}
#endif

static void DirectoryWatch() {
    const auto path = TemporaryName("directory");
    Require(mkdir_nid_postfix(path.c_str(), 0755) == 0);
    const int directory = open_nid_postfix(path.c_str(), Directory, 0);
    Require(directory >= 0);
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(directory);
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite | NoteLink | NoteDelete | NoteRename)) == 0);
    Require(Poll(kq, events, 4) == 0);
    const int child = open_nid_postfix((path + "/child.bin").c_str(), ReadWrite | Create, 0644);
    Require(child >= 0 && close_nid_postfix(child) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == NoteWrite);
    Require(mkdir_nid_postfix((path + "/sub").c_str(), 0755) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].fflags == (NoteWrite | NoteLink));
    Require(sceKernelRename((path + "/child.bin").c_str(), (path + "/renamed.bin").c_str()) == SceOk);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == NoteWrite);
    Require(unlink_nid_postfix((path + "/renamed.bin").c_str()) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 1 && events[0].fflags == NoteWrite);
    Require(rmdir_nid_postfix((path + "/sub").c_str()) == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 1 && events[0].fflags == (NoteWrite | NoteLink));
    Require(close_nid_postfix(directory) == 0);
    Require(close_nid_postfix(kq) == 0);
    Require(std::filesystem::remove_all(path) == 1);
}

static void DeleteWatchedFile() {
    const auto path = TemporaryName("deleted");
    const int file = open_nid_postfix(path.c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(file);
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteDelete | NoteWrite | NoteRename)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(unlink_nid_postfix(path.c_str()) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident && events[0].fflags == NoteDelete);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Require(Poll(kq, events, 4) == 0);
    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(kq) == 0);
    Require(!std::filesystem::exists(path));
}

#ifdef _WIN32
static std::string Utf8(const std::wstring& text) {
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    Require(size > 0);
    std::string narrow(static_cast<std::size_t>(size), '\0');
    Require(::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, narrow.data(), size, nullptr, nullptr) == size);
    narrow.pop_back();
    return narrow;
}

static PACL DenyListing(const std::wstring& directory, PSECURITY_DESCRIPTOR& owner) {
    wchar_t account[256]{};
    DWORD count = static_cast<DWORD>(sizeof(account) / sizeof(account[0]));
    Require(::GetUserNameW(account, &count) != FALSE);
    PACL original = nullptr;
    Require(::GetNamedSecurityInfoW(directory.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &original, nullptr, &owner) == ERROR_SUCCESS);
    _EXPLICIT_ACCESS_W denied{};
    denied.grfAccessPermissions = FILE_LIST_DIRECTORY;
    denied.grfAccessMode = DENY_ACCESS;
    denied.grfInheritance = NO_INHERITANCE;
    denied.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    denied.Trustee.TrusteeType = TRUSTEE_IS_USER;
    denied.Trustee.ptstrName = account;
    PACL blocked = nullptr;
    Require(::SetEntriesInAclW(1, &denied, original, &blocked) == ERROR_SUCCESS);
    const DWORD applied = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, blocked, nullptr);
    ::LocalFree(blocked);
    Require(applied == ERROR_SUCCESS);
    return original;
}

static void RequireContext(const std::string& message, const std::string& context) {
    if (message.find(context) == std::string::npos) {
        std::fprintf(stderr, "the vnode watch error lacks \"%s\": %s\n", context.c_str(), message.c_str());
        std::abort();
    }
}

static void RequireContextInsensitive(std::string message, std::string context) {
    const auto lower = [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); };
    std::transform(message.begin(), message.end(), message.begin(), lower);
    std::transform(context.begin(), context.end(), context.begin(), lower);
    if (message.find(context) == std::string::npos) {
        std::fprintf(stderr, "the vnode watch error lacks \"%s\"\n", context.c_str());
        std::abort();
    }
}

static void DeniedParentWatch() {
    const auto path = TemporaryName("denied");
    Require(mkdir_nid_postfix(path.c_str(), 0755) == 0);
    const int file = open_nid_postfix((path + "/watched.bin").c_str(), ReadWrite | Create, 0644);
    Require(file >= 0);
    PSECURITY_DESCRIPTOR owner = nullptr;
    const auto directory = std::filesystem::absolute(path).wstring();
    const PACL original = DenyListing(directory, owner);
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};
    const auto ident = static_cast<std::uintptr_t>(file);
    const std::string message = ThrownMessage([kq, ident] {
        Apply(kq, Change(ident, EvfiltVnode, EvAdd, NoteWrite));
    });
    const DWORD restored = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(directory.c_str()), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, original, nullptr);
    Require(restored == ERROR_SUCCESS);
    ::LocalFree(owner);
    RequireContext(message, "parent");
    RequireContext(message, "error=5");
    RequireContextInsensitive(message, Utf8(directory));
    Require(Apply(kq, Change(ident, EvfiltVnode, EvAdd | EvClear, NoteWrite)) == 0);
    Require(write_nid_postfix(file, "abc", 3) == 3);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == ident);
    Require((events[0].fflags & NoteWrite) != 0);
    Require(close_nid_postfix(file) == 0);
    Require(close_nid_postfix(kq) == 0);
    Require(std::filesystem::remove_all(path) == 2);
}
#endif
static void Timeout() {
    const int kq = kqueue_nid_postfix();
    KernelEvent event{};
    const KernelTimespec timeout{0, 30000000};
    const auto start = std::chrono::steady_clock::now();
    Require(kevent_nid_postfix(kq, nullptr, 0, &event, 1, &timeout) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(29));
    Require(kevent_nid_postfix(kq, nullptr, 0, &event, 0, nullptr) == 0);
    Require(close_nid_postfix(kq) == 0);
}

static void DisableAndEnable() {
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};

    Require(Apply(kq, Change(10, EvfiltUser, EvAdd | EvDisable)) == 0);
    Require(Apply(kq, Change(10, EvfiltUser, 0, NoteTrigger, 42)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(10, EvfiltUser, EvEnable)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 10 && events[0].data == 42);
    Require(Apply(kq, Change(10, EvfiltUser, EvDelete)) == 0);

    Require(Apply(kq, Change(11, EvfiltUser, EvAdd)) == 0);
    Require(Apply(kq, Change(11, EvfiltUser, 0, NoteTrigger, 99)) == 0);
    Require(Apply(kq, Change(11, EvfiltUser, EvDisable)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(11, EvfiltUser, EvEnable)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 11 && events[0].data == 99);
    Require(Apply(kq, Change(11, EvfiltUser, EvDelete)) == 0);

    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvAdd | EvDisable)) == 0);
    Require(send_nid_postfix(pair[1], "test", 4, 0) == 4);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvEnable)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == static_cast<std::uintptr_t>(pair[0]) && events[0].data == 4);
    char buffer[8]{};
    Require(recv_nid_postfix(pair[0], buffer, sizeof(buffer), 0) == 4);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(kq) == 0);
}

static void Dispatch() {
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};

    Require(Apply(kq, Change(20, EvfiltUser, EvAdd | EvDispatch)) == 0);
    Require(Apply(kq, Change(20, EvfiltUser, 0, NoteTrigger, 55)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 20 && events[0].data == 55 && events[0].flags == EvDispatch);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(20, EvfiltUser, 0, NoteTrigger, 56)) == 0);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(20, EvfiltUser, EvEnable)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 20 && events[0].data == 56 && events[0].flags == EvDispatch);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(20, EvfiltUser, EvDelete)) == 0);

    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvAdd | EvDispatch)) == 0);
    Require(send_nid_postfix(pair[1], "msg", 3, 0) == 3);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == static_cast<std::uintptr_t>(pair[0]) && events[0].data == 3);
    Require(Poll(kq, events, 4) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[0]), EvfiltRead, EvEnable)) == 0);
    Require(Poll(kq, events, 4) == 1);
    char buffer[8]{};
    Require(recv_nid_postfix(pair[0], buffer, sizeof(buffer), 0) == 3);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(kq) == 0);
}

static void ForceOneshot() {
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};

    Require(Apply(kq, Change(30, EvfiltUser, EvAdd | EvForceoneshot)) == 0);
    Require(Apply(kq, Change(30, EvfiltUser, 0, NoteTrigger, 77)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 30 && events[0].data == 77 && events[0].flags == EvForceoneshot);
    RequireFailure(Apply(kq, Change(30, EvfiltUser, EvDelete)), GuestEnoent);

    Require(Apply(kq, Change(31, EvfiltTimer, EvAdd | EvForceoneshot, NoteUseconds, 0)) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].ident == 31 && events[0].data == 1);
    Require(Poll(kq, events, 4) == 0);
    RequireFailure(Apply(kq, Change(31, EvfiltTimer, EvDelete)), GuestEnoent);

    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    Require(Apply(kq, Change(static_cast<std::uintptr_t>(pair[1]), EvfiltWrite, EvAdd | EvForceoneshot)) == 0);
    Require(Wait(kq, events, 4) == 1 && events[0].filter == EvfiltWrite);
    RequireFailure(Apply(kq, Change(static_cast<std::uintptr_t>(pair[1]), EvfiltWrite, EvDelete)), GuestEnoent);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(kq) == 0);
}

static void EdgeCaseFlags() {
    const int kq = kqueue_nid_postfix();
    KernelEvent events[4]{};

    int marker1 = 1;
    int marker2 = 2;
    Require(Apply(kq, Change(40, EvfiltUser, EvAdd | EvDisable, 0, 0, &marker1)) == 0);
    Require(Apply(kq, Change(40, EvfiltUser, 0, NoteTrigger, 100)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(Apply(kq, Change(40, EvfiltUser, 0, 0, 0, &marker2)) == 0);
    Require(Poll(kq, events, 4) == 0);

    Require(Apply(kq, Change(40, EvfiltUser, EvEnable, 0, 0, &marker2)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].ident == 40 && events[0].udata == &marker2 && events[0].data == 100);

    Require(Apply(kq, Change(40, EvfiltUser, EvDispatch)) == 0);
    Require(Apply(kq, Change(40, EvfiltUser, 0, NoteTrigger, 200)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].data == 200 && events[0].flags == EvDispatch);
    Require(Poll(kq, events, 4) == 0);

    Require(Apply(kq, Change(40, EvfiltUser, EvEnable | EvForceoneshot)) == 0);
    Require(Apply(kq, Change(40, EvfiltUser, 0, NoteTrigger, 300)) == 0);
    Require(Poll(kq, events, 4) == 1 && events[0].data == 300 && (events[0].flags & EvForceoneshot) != 0);
    RequireFailure(Apply(kq, Change(40, EvfiltUser, EvDelete)), GuestEnoent);

    Require(close_nid_postfix(kq) == 0);
}

static void StressTest() {
    const int kq = kqueue_nid_postfix();
    int pair[2];
    Require(socketpair_nid_postfix(Unix, Stream, 0, pair) == 0);
    const auto readFd = static_cast<std::uintptr_t>(pair[0]);

    Require(Apply(kq, Change(readFd, EvfiltRead, EvAdd | EvDispatch)) == 0);

    constexpr int TotalMessages = 30;
    std::atomic<int> receivedCount{0};
    std::atomic<int> activeWorkers{0};
    std::atomic<bool> overlapDetected{false};
    std::vector<int> receivedValues;
    std::mutex valuesMutex;

    constexpr int NumWorkers = 4;
    std::vector<std::thread> workers;
    workers.reserve(NumWorkers);

    for (int w = 0; w < NumWorkers; ++w) {
        workers.emplace_back([&, kq, readFd] {
            while (receivedCount.load(std::memory_order_relaxed) < TotalMessages) {
                KernelEvent event{};
                const KernelTimespec timeout{0, 50000000};
                const int count = kevent_nid_postfix(kq, nullptr, 0, &event, 1, &timeout);
                if (count <= 0) {
                    continue;
                }
                const int active = ++activeWorkers;
                if (active > 1) {
                    overlapDetected.store(true, std::memory_order_relaxed);
                }
                Require(event.ident == readFd && event.filter == EvfiltRead);
                int val = 0;
                const auto readBytes = recv_nid_postfix(static_cast<int>(readFd), &val, sizeof(val), 0);
                Require(readBytes == sizeof(val));
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                {
                    std::lock_guard lock(valuesMutex);
                    receivedValues.push_back(val);
                }
                --activeWorkers;
                ++receivedCount;
                Require(Apply(kq, Change(readFd, EvfiltRead, EvEnable)) == 0);
            }
        });
    }

    for (int i = 1; i <= TotalMessages; ++i) {
        Require(send_nid_postfix(pair[1], &i, sizeof(i), 0) == sizeof(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto& worker : workers) {
        worker.join();
    }

    Require(!overlapDetected.load());
    Require(static_cast<int>(receivedValues.size()) == TotalMessages);
    for (int i = 0; i < TotalMessages; ++i) {
        Require(receivedValues[static_cast<std::size_t>(i)] == i + 1);
    }

    Require(Apply(kq, Change(readFd, EvfiltRead, EvDelete)) == 0);
    Require(close_nid_postfix(pair[0]) == 0 && close_nid_postfix(pair[1]) == 0);
    Require(close_nid_postfix(kq) == 0);
}

int main() {
    CreateAndClose();
    Timers();
    UserEvents();
    SocketPair();
    Errors();
    DisableAndEnable();
    Dispatch();
    ForceOneshot();
    EdgeCaseFlags();
    StressTest();
    FileWatch();
#ifndef _WIN32
    RenameAndDeleteWatchedFile();
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
    MoveDirectoryEntries();
#endif
    DirectoryWatch();
    DeleteWatchedFile();
#ifdef _WIN32
    DeniedParentWatch();
#endif
    Timeout();
    return 0;
}

#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#elif defined(__APPLE__)
#include <fcntl.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(__EMSCRIPTEN__)
#include <unistd.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#endif

namespace {

constexpr int GUEST_EBADF = 9;
constexpr int GUEST_EINVAL = 22;
#ifdef __APPLE__
static_assert(NOTE_DELETE == 0x0001 && NOTE_WRITE == 0x0002 && NOTE_EXTEND == 0x0004 && NOTE_ATTRIB == 0x0008 &&
    NOTE_LINK == 0x0010 && NOTE_RENAME == 0x0020);
#else
constexpr std::uint32_t NOTE_DELETE = 0x0001;
constexpr std::uint32_t NOTE_WRITE = 0x0002;
constexpr std::uint32_t NOTE_EXTEND = 0x0004;
constexpr std::uint32_t NOTE_ATTRIB = 0x0008;
constexpr std::uint32_t NOTE_LINK = 0x0010;
constexpr std::uint32_t NOTE_RENAME = 0x0020;
#endif
constexpr std::uint32_t MAPPED_NOTES = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME;

class VnodeWatch {
public:
    VnodeWatch(const KernelEqueueRef& owner, std::uintptr_t descriptor, std::uint32_t notes) : queue(owner), ident(descriptor), interest(notes) {
#ifdef _WIN32
        stopEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent == nullptr) {
            throw std::runtime_error("EVFILT_VNODE: CreateEvent failed, error=" + std::to_string(::GetLastError()));
        }
#elif defined(__APPLE__)
        int ends[2];
        if (::pipe(ends) != 0) {
            throw std::runtime_error("EVFILT_VNODE: pipe failed, errno=" + std::to_string(errno));
        }
        stopEvent = ends[0];
        stopSignal = ends[1];
        ::fcntl(stopEvent, F_SETFD, FD_CLOEXEC);
        ::fcntl(stopSignal, F_SETFD, FD_CLOEXEC);
#elif defined(__EMSCRIPTEN__)
#else
        stopEvent = ::eventfd(0, EFD_CLOEXEC);
        if (stopEvent < 0) {
            throw std::runtime_error("EVFILT_VNODE: eventfd failed, errno=" + std::to_string(errno));
        }
#endif
    }

    ~VnodeWatch() {
#ifdef _WIN32
        ::CloseHandle(stopEvent);
#elif defined(__APPLE__)
        ::close(stopEvent);
        ::close(stopSignal);
#else
        ::close(stopEvent);
#endif
    }

    VnodeWatch(const VnodeWatch&) = delete;
    VnodeWatch& operator=(const VnodeWatch&) = delete;

    void Stop() {
        stopped.store(true);
#ifdef _WIN32
        ::SetEvent(stopEvent);
#elif defined(__APPLE__)
        const char one = 1;
        const auto written = ::write(stopSignal, &one, sizeof(one));
        static_cast<void>(written);
#else
        const std::uint64_t one = 1;
        const auto written = ::write(stopEvent, &one, sizeof(one));
        static_cast<void>(written);
#endif
    }

    void Deliver(std::uint32_t hint);

    std::uint32_t Interest() const { return interest.load(); }
    void SetInterest(std::uint32_t notes) { interest.store(notes); }

#ifdef _WIN32
    HANDLE StopEvent() const { return stopEvent; }
#else
    int StopEvent() const { return stopEvent; }
#endif

private:
    std::weak_ptr<KernelEqueuePrivate> queue;
    std::uintptr_t ident;
    std::atomic<std::uint32_t> interest;
    std::atomic<bool> stopped{false};
#ifdef _WIN32
    HANDLE stopEvent = nullptr;
#elif defined(__APPLE__)
    int stopEvent = -1;
    int stopSignal = -1;
#else
    int stopEvent = -1;
#endif
};

struct VnodeHint {
    const VnodeWatch* watch;
    std::uint32_t hint;
};

void VnodeWatch::Deliver(std::uint32_t hint) {
    if (hint == 0 || stopped.load()) {
        return;
    }
    const auto owner = queue.lock();
    if (!owner) {
        return;
    }
    VnodeHint message{this, hint};
    owner->TriggerEvent(ident, EVFILT_VNODE, &message);
}

#ifdef _WIN32

constexpr DWORD VNODE_POLL_MILLISECONDS = 50;
constexpr DWORD PARENT_CHANGES = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES |
    FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY;

struct Snapshot {
    std::uint64_t size = 0;
    std::uint32_t links = 0;
    std::uint32_t attributes = 0;
    std::int64_t creation = 0;
    std::int64_t lastWrite = 0;
    std::int64_t change = 0;
    bool deletePending = false;
    std::wstring path;
};

bool TakeSnapshot(HANDLE handle, Snapshot& snapshot) {
    std::wstring path(MAX_PATH, L'\0');
    auto length = ::GetFinalPathNameByHandleW(handle, path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
    if (length >= path.size()) {
        path.resize(length);
        length = ::GetFinalPathNameByHandleW(handle, path.data(), length, FILE_NAME_NORMALIZED);
    }
    if (length != 0 && length < path.size()) {
        path.resize(length);
        snapshot.path = std::move(path);
    }
    FILE_BASIC_INFO basic{};
    FILE_STANDARD_INFO standard{};
    FILE_BASIC_INFO settled{};
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (!::GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic)) ||
            !::GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) ||
            !::GetFileInformationByHandleEx(handle, FileBasicInfo, &settled, sizeof(settled))) {
            return false;
        }
        if (settled.LastWriteTime.QuadPart == basic.LastWriteTime.QuadPart && settled.ChangeTime.QuadPart == basic.ChangeTime.QuadPart &&
            settled.CreationTime.QuadPart == basic.CreationTime.QuadPart && settled.FileAttributes == basic.FileAttributes) {
            break;
        }
    }
    snapshot.size = static_cast<std::uint64_t>(standard.EndOfFile.QuadPart);
    snapshot.links = standard.NumberOfLinks;
    snapshot.deletePending = standard.DeletePending != FALSE;
    snapshot.attributes = basic.FileAttributes;
    snapshot.creation = basic.CreationTime.QuadPart;
    snapshot.lastWrite = basic.LastWriteTime.QuadPart;
    snapshot.change = basic.ChangeTime.QuadPart;
    return true;
}

bool IsDeleted(const Snapshot& snapshot) {
    return snapshot.deletePending || snapshot.links == 0 || snapshot.path.find(L"\\$Extend\\$Deleted\\") != std::wstring::npos;
}

std::vector<std::uint32_t> SnapshotHints(const Snapshot& before, const Snapshot& after, bool directory) {
    std::vector<std::uint32_t> hints;
    if (!directory) {
        if (after.size > before.size) {
            hints.push_back(NOTE_WRITE | NOTE_EXTEND);
        } else if (after.size == before.size && after.lastWrite != before.lastWrite) {
            hints.push_back(NOTE_WRITE);
        }
        if (after.links > before.links) {
            hints.push_back(NOTE_LINK);
        }
    }
    if (after.size < before.size || after.attributes != before.attributes || after.creation != before.creation) {
        hints.push_back(NOTE_ATTRIB);
    } else if (!directory && hints.empty() && after.change != before.change) {
        hints.push_back(NOTE_ATTRIB);
    }
    if ((after.deletePending && !before.deletePending) || after.links < before.links || (!IsDeleted(before) && IsDeleted(after))) {
        hints.push_back(NOTE_DELETE);
    }
    if (!after.path.empty() && after.path != before.path && !IsDeleted(after)) {
        hints.push_back(NOTE_RENAME);
    }
    return hints;
}

std::string hostPathText(const std::wstring& path) {
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "<unprintable host path>";
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, text.data(), size, nullptr, nullptr) != size) {
        return "<unprintable host path>";
    }
    text.pop_back();
    return text;
}

std::string changeNotificationFailure(const char* watcher, const std::wstring& path, DWORD error) {
    return "EVFILT_VNODE: the " + std::string(watcher) + " watch of \"" + hostPathText(path) +
        "\" failed, FindFirstChangeNotification error=" + std::to_string(error);
}

HANDLE WatchParent(const std::wstring& path) {
    const auto separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return INVALID_HANDLE_VALUE;
    }
    const auto parent = path.substr(0, separator + 1);
    const HANDLE handle = ::FindFirstChangeNotificationW(parent.c_str(), FALSE, PARENT_CHANGES);
    const DWORD error = handle == INVALID_HANDLE_VALUE ? ::GetLastError() : ERROR_SUCCESS;
    if (handle == INVALID_HANDLE_VALUE && error != ERROR_INVALID_FUNCTION) {
        throw std::runtime_error(changeNotificationFailure("parent", parent, error));
    }
    return handle;
}

HANDLE WatchContents(const std::wstring& path, DWORD changes, const char* watcher) {
    const HANDLE handle = ::FindFirstChangeNotificationW(path.c_str(), FALSE, changes);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        throw std::runtime_error(changeNotificationFailure(watcher, path, error));
    }
    return handle;
}

struct HostWatch {
    HANDLE target = INVALID_HANDLE_VALUE;
    HANDLE parent = INVALID_HANDLE_VALUE;
    HANDLE entries = INVALID_HANDLE_VALUE;
    HANDLE subdirectories = INVALID_HANDLE_VALUE;
    bool directory = false;
    Snapshot last;
};

void CloseChange(HANDLE& handle) {
    if (handle != INVALID_HANDLE_VALUE) {
        ::FindCloseChangeNotification(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

void CloseHostWatch(HostWatch& host) {
    CloseChange(host.parent);
    CloseChange(host.entries);
    CloseChange(host.subdirectories);
    if (host.target != INVALID_HANDLE_VALUE) {
        ::CloseHandle(host.target);
        host.target = INVALID_HANDLE_VALUE;
    }
}

bool Signaled(HANDLE handle) {
    return handle != INVALID_HANDLE_VALUE && ::WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
}

void RunHostWatch(std::shared_ptr<VnodeWatch> watch, HostWatch host) {
    for (;;) {
        std::vector<HANDLE> handles{watch->StopEvent()};
        for (const HANDLE handle : {host.parent, host.entries, host.subdirectories}) {
            if (handle != INVALID_HANDLE_VALUE) {
                handles.push_back(handle);
            }
        }
        const DWORD result = ::WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, VNODE_POLL_MILLISECONDS);
        if (result == WAIT_OBJECT_0) {
            break;
        }
        if (result == WAIT_FAILED) {
            CloseHostWatch(host);
            throw std::runtime_error("EVFILT_VNODE: WaitForMultipleObjects failed, error=" + std::to_string(::GetLastError()));
        }
        std::vector<std::uint32_t> hints;
        if (Signaled(host.entries)) {
            hints.push_back(NOTE_WRITE);
            ::FindNextChangeNotification(host.entries);
        }
        if (Signaled(host.subdirectories)) {
            hints.push_back(NOTE_WRITE | NOTE_LINK);
            ::FindNextChangeNotification(host.subdirectories);
        }
        if (Signaled(host.parent)) {
            ::FindNextChangeNotification(host.parent);
        }
        Snapshot current = host.last;
        if (TakeSnapshot(host.target, current)) {
            for (const auto hint : SnapshotHints(host.last, current, host.directory)) {
                hints.push_back(hint);
            }
            if (current.path != host.last.path) {
                CloseChange(host.parent);
                if (!IsDeleted(current)) {
                    host.parent = WatchParent(current.path);
                }
            }
            host.last = std::move(current);
        }
        for (const auto hint : hints) {
            watch->Deliver(hint);
        }
    }
    CloseHostWatch(host);
}

int OpenHostWatch(int descriptor, std::uint32_t notes, HostWatch& host) {
    if (const auto directory = File::DirectoryDescriptorPath(descriptor)) {
        host.directory = true;
        host.target = ::CreateFileW(directory->wstring().c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    } else {
        const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
        if (handle == INVALID_HANDLE_VALUE) {
            return GUEST_EBADF;
        }
        const DWORD type = ::GetFileType(handle);
        if (type == FILE_TYPE_PIPE) {
            return GUEST_EINVAL;
        }
        if (type != FILE_TYPE_DISK) {
            NotImplemented_nid_no_patch("EVFILT_VNODE: a descriptor that is not a file or a directory");
        }
        host.target = ::ReOpenFile(handle, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    }
    if (host.target == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("EVFILT_VNODE: opening the watched file failed, error=" + std::to_string(::GetLastError()));
    }
    if (!TakeSnapshot(host.target, host.last) || host.last.path.empty()) {
        const auto error = ::GetLastError();
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: reading the watched file failed, error=" + std::to_string(error));
    }
    try {
        if (!IsDeleted(host.last)) {
            host.parent = WatchParent(host.last.path);
        }
        if (host.directory) {
            host.entries = WatchContents(host.last.path, FILE_NOTIFY_CHANGE_FILE_NAME, "entries");
            host.subdirectories = WatchContents(host.last.path, FILE_NOTIFY_CHANGE_DIR_NAME, "subdirectories");
        }
    } catch (...) {
        CloseHostWatch(host);
        throw;
    }
    return 0;
}

#elif defined(__APPLE__)

struct HostWatch {
    int notify = -1;
    int target = -1;
};

void CloseHostWatch(HostWatch& host) {
    for (int* descriptor : {&host.notify, &host.target}) {
        if (*descriptor >= 0) {
            ::close(*descriptor);
            *descriptor = -1;
        }
    }
}

void RunHostWatch(std::shared_ptr<VnodeWatch> watch, HostWatch host) {
    struct kevent stop {};
    EV_SET(&stop, watch->StopEvent(), EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (::kevent(host.notify, &stop, 1, nullptr, 0, nullptr) < 0) {
        const int error = errno;
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: registering the stop event failed, errno=" + std::to_string(error));
    }
    for (;;) {
        struct kevent event {};
        if (::kevent(host.notify, nullptr, 0, &event, 1, nullptr) < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int error = errno;
            CloseHostWatch(host);
            throw std::runtime_error("EVFILT_VNODE: kevent failed, errno=" + std::to_string(error));
        }
        if (event.filter != EVFILT_VNODE) {
            break;
        }
        auto notes = event.fflags & MAPPED_NOTES;
        if ((notes & NOTE_DELETE) != 0) {
            notes &= ~NOTE_LINK;
        }
        watch->Deliver(notes);
    }
    CloseHostWatch(host);
}

int OpenHostWatch(int descriptor, std::uint32_t, HostWatch& host) {
    struct stat status {};
    if (::fstat(descriptor, &status) != 0) {
        return GUEST_EBADF;
    }
    if (S_ISFIFO(status.st_mode) || S_ISSOCK(status.st_mode)) {
        return GUEST_EINVAL;
    }
    if (!S_ISREG(status.st_mode) && !S_ISDIR(status.st_mode)) {
        NotImplemented_nid_no_patch("EVFILT_VNODE: a descriptor that is not a file or a directory");
    }
    host.target = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
    if (host.target < 0) {
        const int error = errno;
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: duplicating the watched descriptor failed, errno=" + std::to_string(error));
    }
    host.notify = ::kqueue();
    struct kevent change {};
    EV_SET(&change, host.target, EVFILT_VNODE, EV_ADD | EV_CLEAR, MAPPED_NOTES, 0, nullptr);
    if (host.notify < 0 || ::kevent(host.notify, &change, 1, nullptr, 0, nullptr) < 0) {
        const int error = errno;
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: kqueue failed, errno=" + std::to_string(error));
    }
    return 0;
}

#elif defined(__EMSCRIPTEN__)

struct HostWatch {};

void CloseHostWatch(HostWatch&) {}

void RunHostWatch(std::shared_ptr<VnodeWatch>, HostWatch) {}

int OpenHostWatch(int, std::uint32_t, HostWatch&) {
    NotImplemented_nid_no_patch("EVFILT_VNODE in WebAssembly");
    return GUEST_EINVAL;
}

#else

struct HostWatch {
    int notify = -1;
    int target = -1;
    int entries = -1;
    bool directory = false;
    struct stat last {};
};

struct HostEvent {
    std::uint32_t mask;
    std::uint32_t cookie;
};

void CloseHostWatch(HostWatch& host) {
    for (int* descriptor : {&host.notify, &host.target, &host.entries}) {
        if (*descriptor >= 0) {
            ::close(*descriptor);
            *descriptor = -1;
        }
    }
}

std::uint32_t AttributeHint(HostWatch& host) {
    struct stat current {};
    if (::fstat(host.target, &current) != 0) {
        return NOTE_ATTRIB;
    }
    std::uint32_t hint = NOTE_ATTRIB;
    if (current.st_nlink > host.last.st_nlink) {
        hint = NOTE_LINK;
    } else if (current.st_nlink < host.last.st_nlink) {
        hint = host.directory && current.st_nlink != 0 ? NOTE_LINK : NOTE_DELETE;
    }
    host.last = current;
    return hint;
}

std::uint32_t ModifyHint(HostWatch& host) {
    struct stat current {};
    if (::fstat(host.target, &current) != 0) {
        return NOTE_WRITE;
    }
    std::uint32_t hint = NOTE_WRITE;
    if (current.st_size > host.last.st_size) {
        hint = NOTE_WRITE | NOTE_EXTEND;
    } else if (current.st_size < host.last.st_size) {
        hint = NOTE_ATTRIB;
    }
    host.last = current;
    return hint;
}

int ReadHostEvents(const HostWatch& host, std::vector<HostEvent>& events) {
    alignas(inotify_event) char buffer[16384];
    for (;;) {
        const auto length = ::read(host.notify, buffer, sizeof(buffer));
        if (length < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno == EAGAIN ? 0 : errno;
        }
        if (length == 0) {
            return 0;
        }
        for (char* cursor = buffer; cursor < buffer + length;) {
            const auto* event = reinterpret_cast<const inotify_event*>(cursor);
            events.push_back({event->mask, event->cookie});
            cursor += sizeof(inotify_event) + event->len;
        }
    }
}

std::unordered_map<std::uint32_t, int> CountMoves(const std::vector<HostEvent>& events) {
    std::unordered_map<std::uint32_t, int> moves;
    for (const auto& event : events) {
        if ((event.mask & (IN_MOVED_FROM | IN_MOVED_TO)) != 0) {
            ++moves[event.cookie];
        }
    }
    return moves;
}

bool HasUnpairedMove(const std::vector<HostEvent>& events, std::size_t first) {
    const auto moves = CountMoves(events);
    for (std::size_t index = first; index < events.size(); ++index) {
        if ((events[index].mask & IN_MOVED_FROM) != 0 && moves.at(events[index].cookie) == 1) {
            return true;
        }
    }
    return false;
}

int WaitForRenamesInProgress(const HostWatch& host) {
    char entries[1024];
    if (::getdents64(host.entries, entries, sizeof(entries)) < 0 && errno != ENOENT) {
        return errno;
    }
    return 0;
}

int ReadSettledHostEvents(const HostWatch& host, std::vector<HostEvent>& events) {
    if (const int error = ReadHostEvents(host, events)) {
        return error;
    }
    for (std::size_t settled = 0; HasUnpairedMove(events, settled);) {
        settled = events.size();
        if (const int error = WaitForRenamesInProgress(host)) {
            return error;
        }
        if (const int error = ReadHostEvents(host, events)) {
            return error;
        }
    }
    return 0;
}

void RunHostWatch(std::shared_ptr<VnodeWatch> watch, HostWatch host) {
    pollfd waits[2] = {{host.notify, POLLIN, 0}, {watch->StopEvent(), POLLIN, 0}};
    bool watching = true;
    while (watching) {
        if (::poll(waits, 2, -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            CloseHostWatch(host);
            throw std::runtime_error("EVFILT_VNODE: poll failed, errno=" + std::to_string(errno));
        }
        if (waits[1].revents != 0) {
            break;
        }
        std::vector<HostEvent> events;
        if (const int error = ReadSettledHostEvents(host, events)) {
            CloseHostWatch(host);
            throw std::runtime_error("EVFILT_VNODE: reading inotify failed, errno=" + std::to_string(error));
        }
        const auto moves = CountMoves(events);
        std::vector<std::uint32_t> hints;
        for (const auto& event : events) {
            const std::uint32_t link = (event.mask & IN_ISDIR) != 0 ? NOTE_LINK : 0;
            if ((event.mask & IN_MODIFY) != 0) {
                hints.push_back(ModifyHint(host));
            }
            if ((event.mask & IN_ATTRIB) != 0) {
                hints.push_back(AttributeHint(host));
            }
            if ((event.mask & (IN_CREATE | IN_DELETE)) != 0) {
                hints.push_back(NOTE_WRITE | link);
            }
            if ((event.mask & (IN_MOVED_FROM | IN_MOVED_TO)) != 0) {
                hints.push_back(moves.at(event.cookie) > 1 ? NOTE_WRITE : NOTE_WRITE | NOTE_EXTEND | link);
            }
            if ((event.mask & IN_MOVE_SELF) != 0) {
                hints.push_back(NOTE_RENAME);
            }
            if ((event.mask & IN_DELETE_SELF) != 0) {
                hints.push_back(NOTE_DELETE);
            }
            if ((event.mask & IN_IGNORED) != 0) {
                watching = false;
            }
        }
        for (const auto hint : hints) {
            watch->Deliver(hint);
        }
    }
    CloseHostWatch(host);
}

int OpenHostWatch(int descriptor, std::uint32_t, HostWatch& host) {
    struct stat status {};
    if (::fstat(descriptor, &status) != 0) {
        return GUEST_EBADF;
    }
    if (S_ISFIFO(status.st_mode) || S_ISSOCK(status.st_mode)) {
        return GUEST_EINVAL;
    }
    if (!S_ISREG(status.st_mode) && !S_ISDIR(status.st_mode)) {
        NotImplemented_nid_no_patch("EVFILT_VNODE: a descriptor that is not a file or a directory");
    }
    host.directory = S_ISDIR(status.st_mode);
    const auto path = "/proc/self/fd/" + std::to_string(descriptor);
    host.target = ::open(path.c_str(), O_PATH | O_CLOEXEC);
    if (host.target < 0 || ::fstat(host.target, &host.last) != 0) {
        const int error = errno;
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: opening the watched file failed, errno=" + std::to_string(error));
    }
    if (host.directory) {
        host.entries = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (host.entries < 0) {
            const int error = errno;
            CloseHostWatch(host);
            throw std::runtime_error("EVFILT_VNODE: opening the watched directory failed, errno=" + std::to_string(error));
        }
    }
    host.notify = ::inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    std::uint32_t mask = IN_MODIFY | IN_ATTRIB | IN_MOVE_SELF | IN_DELETE_SELF;
    if (host.directory) {
        mask |= IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
    }
    if (host.notify < 0 || ::inotify_add_watch(host.notify, path.c_str(), mask) < 0) {
        const int error = errno;
        CloseHostWatch(host);
        throw std::runtime_error("EVFILT_VNODE: inotify failed, errno=" + std::to_string(error));
    }
    return 0;
}

#endif

void TriggerVnode(KernelEqueueEvent* event, void* data) {
    const auto* message = static_cast<const VnodeHint*>(data);
    if (event->filter.data != message->watch) {
        return;
    }
    if ((message->watch->Interest() & message->hint) != 0) {
        event->event.fflags |= message->hint;
    }
    event->triggered = event->event.fflags != 0;
}

void StopVnode(KernelEqueue, KernelEqueueEvent* event) {
    static_cast<VnodeWatch*>(event->filter.data)->Stop();
}

int VnodeDescriptorError(int descriptor) {
    if (descriptor < 0) {
        return GUEST_EBADF;
    }
    if (GuestSockets::IsOpen(descriptor)) {
        return GUEST_EINVAL;
    }
    if (!DescriptorIsOpen_nid_no_patch(descriptor)) {
        return GUEST_EBADF;
    }
    return IsKqueueDescriptor_nid_no_patch(descriptor) ? GUEST_EINVAL : 0;
}

}

void RequireVnodeNotes(std::uint32_t fflags) {
    if ((fflags & ~MAPPED_NOTES) != 0) {
        NotImplemented_nid_no_patch(("EVFILT_VNODE: fflags " + std::to_string(fflags & ~MAPPED_NOTES)).c_str());
    }
}

void SetVnodeNotes(KernelEqueueEvent& event, std::uint32_t fflags) {
    static_cast<VnodeWatch*>(event.filter.data)->SetInterest(fflags);
}

int AddVnodeEvent(const KernelEqueueRef& queue, int descriptor, std::uint32_t fflags, std::uint16_t flags, void* udata) {
    RequireVnodeNotes(fflags);
    if (const int error = VnodeDescriptorError(descriptor)) {
        return error;
    }
    HostWatch host;
    if (const int error = OpenHostWatch(descriptor, fflags, host)) {
        return error;
    }
    std::shared_ptr<VnodeWatch> watch;
    try {
        watch = std::make_shared<VnodeWatch>(queue, static_cast<std::uintptr_t>(descriptor), fflags);
    } catch (...) {
        CloseHostWatch(host);
        throw;
    }
    KernelEqueueEvent event{};
    event.event.ident = static_cast<std::uintptr_t>(descriptor);
    event.event.filter = EVFILT_VNODE;
    event.event.flags = flags;
    event.event.udata = udata;
    event.filter.data = watch.get();
    event.filter.owner = watch;
    event.filter.triggerFunc = TriggerVnode;
    event.filter.deleteEventFunc = StopVnode;
    const int result = queue->AddEvent(event);
    bool registered = false;
    if (result == EQUEUE_OK) {
        queue->EditEvent(event.event.ident, EVFILT_VNODE, [&registered, &watch](KernelEqueueEvent& added) {
            registered = added.filter.data == watch.get();
        });
    }
    if (!registered) {
        CloseHostWatch(host);
        return result == EQUEUE_OK ? 0 : result & 0xffff;
    }
    std::thread(RunHostWatch, watch, std::move(host)).detach();
    return 0;
}

extern "C" {

int APS5_VABI sceKernelAddFileEvent(KernelEqueue eq, int fd, int watch, void* udata) {
    const auto queue = EqueuePin_nid_postfix(eq);
    if (!queue) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    const auto notes = static_cast<std::uint32_t>(watch);
    RequireVnodeNotes(notes);
    if (const int error = VnodeDescriptorError(fd)) {
        return SceKernelError(error);
    }
    const int edited = queue->EditEvent(static_cast<std::uintptr_t>(fd), EVFILT_VNODE, [notes, udata](KernelEqueueEvent& event) {
        event.event.udata = udata;
        SetVnodeNotes(event, notes);
    });
    if (edited != SCE_KERNEL_ERROR_ENOENT) {
        return edited;
    }
    const int error = AddVnodeEvent(queue, fd, notes, EV_CLEAR, udata);
    return error == 0 ? EQUEUE_OK : SceKernelError(error);
}

int APS5_VABI sceKernelDeleteFileEvent(KernelEqueue eq, int fd) {
    if (!EqueuePin_nid_postfix(eq)) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    if (fd < 0 || (!GuestSockets::IsOpen(fd) && !DescriptorIsOpen_nid_no_patch(fd))) {
        return SCE_KERNEL_ERROR_EBADF;
    }
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<std::uintptr_t>(fd), EVFILT_VNODE);
}

}

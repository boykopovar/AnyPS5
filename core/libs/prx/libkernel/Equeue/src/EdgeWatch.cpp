#include "prx/libkernel/Equeue/include/EdgeWatch.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <sys/event.h>
#include <unistd.h>
#endif

namespace EdgeWatch {

#ifdef __linux__

std::shared_ptr<Watch> Watch::Open(std::intptr_t hostDescriptor, bool write) {
    const int descriptor = ::epoll_create1(EPOLL_CLOEXEC);
    if (descriptor < 0) {
        throw std::runtime_error("kevent: EV_CLEAR: epoll_create1 failed, host errno " + std::to_string(errno));
    }
    auto watch = std::make_shared<Watch>(descriptor);
    epoll_event interest{};
    interest.events = EPOLLET | (write ? EPOLLOUT : (EPOLLIN | EPOLLRDHUP));
    if (::epoll_ctl(descriptor, EPOLL_CTL_ADD, static_cast<int>(hostDescriptor), &interest) != 0) {
        throw std::runtime_error("kevent: EV_CLEAR: watching host descriptor " + std::to_string(hostDescriptor) + " failed, host errno " + std::to_string(errno));
    }
    return watch;
}

Watch::~Watch() {
    ::close(m_descriptor);
}

bool Watch::TakeEdge() {
    epoll_event ready{};
    return ::epoll_wait(m_descriptor, &ready, 1, 0) > 0;
}

#elif defined(__APPLE__)

std::shared_ptr<Watch> Watch::Open(std::intptr_t hostDescriptor, bool write) {
    auto watch = std::make_shared<Watch>(-1);
    const int descriptor = ::kqueue();
    if (descriptor < 0) {
        throw std::runtime_error("kevent: EV_CLEAR: kqueue failed, host errno " + std::to_string(errno));
    }
    watch->m_descriptor = descriptor;
    if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        throw std::runtime_error("kevent: EV_CLEAR: setting close-on-exec failed, host errno " + std::to_string(errno));
    }
    struct kevent interest{};
    EV_SET(&interest, static_cast<std::uintptr_t>(hostDescriptor), write ? EVFILT_WRITE : EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (::kevent(descriptor, &interest, 1, nullptr, 0, nullptr) != 0) {
        throw std::runtime_error("kevent: EV_CLEAR: watching host descriptor " + std::to_string(hostDescriptor) + " failed, host errno " + std::to_string(errno));
    }
    return watch;
}

Watch::~Watch() {
    if (m_descriptor >= 0) ::close(m_descriptor);
}

bool Watch::TakeEdge() {
    struct kevent ready{};
    const timespec timeout{};
    int result;
    do {
        result = ::kevent(m_descriptor, nullptr, 0, &ready, 1, &timeout);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        throw std::runtime_error("kevent: EV_CLEAR: retrieving host event failed, host errno " + std::to_string(errno));
    }
    return result > 0;
}

#else

std::shared_ptr<Watch> Watch::Open(std::intptr_t, bool) {
    NotImplemented_nid_no_patch("kevent: EV_CLEAR on a socket or a pipe off Linux");
    return nullptr;
}

Watch::~Watch() = default;

bool Watch::TakeEdge() {
    return false;
}

#endif

}

#include "prx/libkernel/Equeue/include/EdgeWatch.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>

#ifdef __linux__
#include <sys/epoll.h>
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

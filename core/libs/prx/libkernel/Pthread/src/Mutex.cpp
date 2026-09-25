#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <string>

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x8002000B;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x8002003C;

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadMutexattrInit: null attr");
    auto* p = new (std::nothrow) PthreadMutexattrPrivate{MutexType::Normal};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrSettype: null attr");
    switch (type) {
    case 1: (*attr)->type = MutexType::ErrorCheck; break;
    case 2: (*attr)->type = MutexType::Recursive; break;
    case 3: (*attr)->type = MutexType::Normal; break;
    case 4: (*attr)->type = MutexType::Normal; break;
    default: throw std::runtime_error("scePthreadMutexattrSettype: invalid type " + std::to_string(type));
    }
    return SCE_OK;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    if (!mutex) throw std::runtime_error("scePthreadMutexInit: null mutex");
    MutexType t = MutexType::Normal;
    if (attr && *attr) t = (*attr)->type;
    auto* p = new (std::nothrow) PthreadMutexPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_type = t;
    *mutex = p;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexDestroy: null mutex");
    delete *mutex;
    *mutex = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexLock: null mutex");
    auto* m = *mutex;
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive) {
        m->_rmtx.lock();
        m->_owner.store(tid, std::memory_order_relaxed);
        ++m->_count;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck) {
        if (m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    }
    m->_mtx.lock();
    m->_owner.store(tid, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexUnlock: null mutex");
    auto* m = *mutex;
    if (m->_type == MutexType::ErrorCheck || m->_type == MutexType::Normal) {
        if (m->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
            return SCE_KERNEL_ERROR_EPERM;
    }
    if (m->_type == MutexType::Recursive) {
        if (--m->_count == 0) m->_owner.store(std::thread::id{}, std::memory_order_relaxed);
        m->_rmtx.unlock();
        return SCE_OK;
    }
    m->_owner.store(std::thread::id{}, std::memory_order_relaxed);
    m->_mtx.unlock();
    return SCE_OK;
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexTimedlock: null mutex");
    auto* m = *mutex;
    const auto tid = std::this_thread::get_id();
    const auto timeout = std::chrono::microseconds(usec);
    if (m->_type == MutexType::Recursive) {
        if (!m->_rmtx.try_lock_for(timeout)) return SCE_KERNEL_ERROR_ETIMEDOUT;
        m->_owner.store(tid, std::memory_order_relaxed);
        ++m->_count;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck) {
        if (m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    }
    if (!m->_mtx.try_lock_for(timeout)) return SCE_KERNEL_ERROR_ETIMEDOUT;
    m->_owner.store(tid, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexTrylock: null mutex");
    auto* m = *mutex;
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive) {
        if (!m->_rmtx.try_lock()) return SCE_KERNEL_ERROR_EBUSY;
        m->_owner.store(tid, std::memory_order_relaxed);
        ++m->_count;
        return SCE_OK;
    }
    if (!m->_mtx.try_lock()) return SCE_KERNEL_ERROR_EBUSY;
    m->_owner.store(tid, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
 (void)attr;
 (void)protocol;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

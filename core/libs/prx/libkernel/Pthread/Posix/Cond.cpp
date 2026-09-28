#include <cstdint>
#include <cstddef>
#include <cerrno>
#include <new>
#include "SceTypes.hpp"
#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
 if (!cond || !*cond) return EINVAL;
 (*cond)->_cv.notify_all();
 return 0;
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
 if (!cond || (attr && !*attr)) return EINVAL;
 auto* instance = new (std::nothrow) PthreadCondPrivate{};
 if (!instance) return ENOMEM;
 *cond = instance;
 return 0;
}

int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond) {
 if (!cond || !*cond) return EINVAL;
 if ((*cond)->_waiters.load(std::memory_order_acquire)) return EBUSY;
 delete *cond;
 *cond = nullptr;
 return 0;
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
 if (!cond || !*cond) return EINVAL;
 (*cond)->_cv.notify_one();
 return 0;
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
 (void)cond;
 (void)mutex;
 (void)abstime;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
 if (!cond || !*cond || !mutex || !*mutex) return EINVAL;
 auto* c = *cond;
 auto* m = *mutex;
 const auto thread = std::this_thread::get_id();
 if (m->_owner.load(std::memory_order_acquire) != thread ||
     (m->_type == MutexType::Recursive && m->_count != 1)) return EPERM;
 c->_waiters.fetch_add(1, std::memory_order_acq_rel);
 try {
     if (m->_type == MutexType::Recursive) {
         std::unique_lock<std::recursive_timed_mutex> lock(m->_rmtx, std::adopt_lock);
         c->_cv.wait(lock);
         lock.release();
     } else {
         std::unique_lock<std::timed_mutex> lock(m->_mtx, std::adopt_lock);
         c->_cv.wait(lock);
         lock.release();
     }
 } catch (...) {
     c->_waiters.fetch_sub(1, std::memory_order_acq_rel);
     return EINVAL;
 }
 m->_owner.store(thread, std::memory_order_release);
 c->_waiters.fetch_sub(1, std::memory_order_acq_rel);
 return 0;
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
 (void)attr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
 (void)attr;
 (void)clock_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

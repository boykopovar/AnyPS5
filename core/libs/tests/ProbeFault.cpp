#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

static int Call(int index, int variant) {
  switch (index) {
    case 0: { std::fprintf(stderr, "__tls_get_addr\n"); (void)__tls_get_addr_nid_postfix((const KernelTlsIndex*)0x10); return 0; }
    case 1: { std::fprintf(stderr, "sceKernelMlock\n"); (void)sceKernelMlock_nid_postfix((void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 2: { std::fprintf(stderr, "sceKernelMunlock\n"); (void)sceKernelMunlock_nid_postfix((void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 3: { std::fprintf(stderr, "mmap\n"); (void)mmap_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 4: { std::fprintf(stderr, "munmap\n"); (void)munmap_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 5: { std::fprintf(stderr, "mprotect\n"); (void)mprotect_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 6: { std::fprintf(stderr, "mlock\n"); (void)mlock_nid_postfix((const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 7: { std::fprintf(stderr, "munlock\n"); (void)munlock_nid_postfix((const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 8: { std::fprintf(stderr, "msync\n"); (void)msync_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 9: { std::fprintf(stderr, "madvise\n"); (void)madvise_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 10: { std::fprintf(stderr, "posix_madvise\n"); (void)posix_madvise_nid_postfix((void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 11: { std::fprintf(stderr, "mincore\n"); (void)mincore_nid_postfix((const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (char*)0x10); return 0; }
    case 12: { std::fprintf(stderr, "kevent\n"); (void)kevent_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelEvent*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelEvent*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelTimespec*)0x10); return 0; }
    case 13: { std::fprintf(stderr, "shm_open\n"); (void)shm_open_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 14: { std::fprintf(stderr, "shm_unlink\n"); (void)shm_unlink_nid_postfix((const char*)0x10); return 0; }
    case 15: { std::fprintf(stderr, "pipe\n"); (void)pipe_nid_postfix((int*)0x10); return 0; }
    case 16: { std::fprintf(stderr, "chmod\n"); (void)chmod_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 17: { std::fprintf(stderr, "chflags\n"); (void)chflags_nid_postfix((const char*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 18: { std::fprintf(stderr, "nmount\n"); (void)nmount_nid_postfix((const KernelIovec*)0x10, (unsigned int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 19: { std::fprintf(stderr, "_fstatfs\n"); (void)_fstatfs_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelStatfs*)0x10); return 0; }
    case 20: { std::fprintf(stderr, "fstat_nid_disambig1\n"); (void)fstat_nid_disambig1_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (FileStat*)0x10); return 0; }
    case 21: { std::fprintf(stderr, "_fstat\n"); (void)_fstat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (FileStat*)0x10); return 0; }
    case 22: { std::fprintf(stderr, "mkdir\n"); (void)mkdir_nid_postfix((const char*)0x10, (uint16_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 23: { std::fprintf(stderr, "open\n"); (void)open_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 24: { std::fprintf(stderr, "creat\n"); (void)creat_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 25: { std::fprintf(stderr, "pread\n"); (void)pread_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 26: { std::fprintf(stderr, "pwrite_nid_disambig1\n"); (void)pwrite_nid_disambig1_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 27: { std::fprintf(stderr, "read\n"); (void)read_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 28: { std::fprintf(stderr, "_read\n"); (void)_read_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 29: { std::fprintf(stderr, "write\n"); (void)write_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 30: { std::fprintf(stderr, "_write\n"); (void)_write_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 31: { std::fprintf(stderr, "stat\n"); (void)stat_nid_postfix((const char*)0x10, (FileStat*)0x10); return 0; }
    case 32: { std::fprintf(stderr, "lstat\n"); (void)lstat_nid_postfix((const char*)0x10, (FileStat*)0x10); return 0; }
    case 33: { std::fprintf(stderr, "unlink\n"); (void)unlink_nid_postfix((const char*)0x10); return 0; }
    case 34: { std::fprintf(stderr, "getdirentries\n"); (void)getdirentries_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t*)0x10); return 0; }
    case 35: { std::fprintf(stderr, "_getdirentries\n"); (void)_getdirentries_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t*)0x10); return 0; }
    case 36: { std::fprintf(stderr, "getdents\n"); (void)getdents_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 37: { std::fprintf(stderr, "preadv\n"); (void)preadv_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 38: { std::fprintf(stderr, "pwritev\n"); (void)pwritev_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 39: { std::fprintf(stderr, "rmdir\n"); (void)rmdir_nid_postfix((const char*)0x10); return 0; }
    case 40: { std::fprintf(stderr, "sceKernelChmod\n"); (void)sceKernelChmod_nid_postfix((const char*)0x10, (std::uint16_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 41: { std::fprintf(stderr, "sceKernelTruncate\n"); (void)sceKernelTruncate_nid_postfix((const char*)0x10, (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 42: { std::fprintf(stderr, "truncate\n"); (void)truncate_nid_postfix((const char*)0x10, (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 43: { std::fprintf(stderr, "sceKernelUtimes\n"); (void)sceKernelUtimes_nid_postfix((const char*)0x10, (const KernelTimeval*)0x10); return 0; }
    case 44: { std::fprintf(stderr, "utimes\n"); (void)utimes_nid_postfix((const char*)0x10, (const KernelTimeval*)0x10); return 0; }
    case 45: { std::fprintf(stderr, "lutimes\n"); (void)lutimes_nid_postfix((const char*)0x10, (const KernelTimeval*)0x10); return 0; }
    case 46: { std::fprintf(stderr, "futimes\n"); (void)futimes_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelTimeval*)0x10); return 0; }
    case 47: { std::fprintf(stderr, "readv\n"); (void)readv_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 48: { std::fprintf(stderr, "writev\n"); (void)writev_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 49: { std::fprintf(stderr, "_readv\n"); (void)_readv_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 50: { std::fprintf(stderr, "_writev\n"); (void)_writev_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelIovec*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 51: { std::fprintf(stderr, "cfsetspeed\n"); (void)cfsetspeed_nid_postfix((void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 52: { std::fprintf(stderr, "tcgetattr\n"); (void)tcgetattr_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 53: { std::fprintf(stderr, "tcsetattr\n"); (void)tcsetattr_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10); return 0; }
    case 54: { std::fprintf(stderr, "dlopen\n"); (void)dlopen_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 55: { std::fprintf(stderr, "dlsym\n"); (void)dlsym_nid_postfix((void*)0x10, (const char*)0x10); return 0; }
    case 56: { std::fprintf(stderr, "__elf_phdr_match_addr\n"); (void)__elf_phdr_match_addr_nid_postfix((dl_phdr_info*)0x10, (void*)0x10); return 0; }
    case 57: { std::fprintf(stderr, "sceKernelGetLibkernelTextLocation\n"); (void)sceKernelGetLibkernelTextLocation_nid_postfix((void**)0x10, (std::size_t*)0x10); return 0; }
    case 58: { std::fprintf(stderr, "sceKernelInternalMemoryGetModuleSegmentInfo\n"); (void)sceKernelInternalMemoryGetModuleSegmentInfo_nid_postfix((InternalSegmentInfo*)0x10); return 0; }
    case 59: { std::fprintf(stderr, "pthread_attr_destroy\n"); (void)pthread_attr_destroy_nid_postfix((PthreadAttr*)0x10); return 0; }
    case 60: { std::fprintf(stderr, "pthread_attr_get_np\n"); (void)pthread_attr_get_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (PthreadAttr*)0x10); return 0; }
    case 61: { std::fprintf(stderr, "pthread_attr_getdetachstate\n"); (void)pthread_attr_getdetachstate_nid_postfix((const PthreadAttr*)0x10, (int*)0x10); return 0; }
    case 62: { std::fprintf(stderr, "pthread_attr_getguardsize\n"); (void)pthread_attr_getguardsize_nid_postfix((const PthreadAttr*)0x10, (size_t*)0x10); return 0; }
    case 63: { std::fprintf(stderr, "pthread_attr_getinheritsched\n"); (void)pthread_attr_getinheritsched_nid_postfix((const PthreadAttr*)0x10, (int*)0x10); return 0; }
    case 64: { std::fprintf(stderr, "pthread_attr_getschedparam\n"); (void)pthread_attr_getschedparam_nid_postfix((const PthreadAttr*)0x10, (KernelSchedParam*)0x10); return 0; }
    case 65: { std::fprintf(stderr, "pthread_attr_getschedpolicy\n"); (void)pthread_attr_getschedpolicy_nid_postfix((const PthreadAttr*)0x10, (int*)0x10); return 0; }
    case 66: { std::fprintf(stderr, "pthread_attr_getscope\n"); (void)pthread_attr_getscope_nid_postfix((const PthreadAttr*)0x10, (int*)0x10); return 0; }
    case 67: { std::fprintf(stderr, "pthread_attr_getstack\n"); (void)pthread_attr_getstack_nid_postfix((const PthreadAttr*)0x10, (void**)0x10, (size_t*)0x10); return 0; }
    case 68: { std::fprintf(stderr, "pthread_attr_getstackaddr\n"); (void)pthread_attr_getstackaddr_nid_postfix((const PthreadAttr*)0x10, (void**)0x10); return 0; }
    case 69: { std::fprintf(stderr, "pthread_attr_getstacksize\n"); (void)pthread_attr_getstacksize_nid_postfix((const PthreadAttr*)0x10, (size_t*)0x10); return 0; }
    case 70: { std::fprintf(stderr, "pthread_attr_init\n"); (void)pthread_attr_init_nid_postfix((PthreadAttr*)0x10); return 0; }
    case 71: { std::fprintf(stderr, "pthread_attr_setdetachstate\n"); (void)pthread_attr_setdetachstate_nid_postfix((PthreadAttr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 72: { std::fprintf(stderr, "pthread_attr_setguardsize\n"); (void)pthread_attr_setguardsize_nid_postfix((PthreadAttr*)0x10, (size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 73: { std::fprintf(stderr, "pthread_attr_setinheritsched\n"); (void)pthread_attr_setinheritsched_nid_postfix((PthreadAttr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 74: { std::fprintf(stderr, "pthread_attr_setschedparam\n"); (void)pthread_attr_setschedparam_nid_postfix((PthreadAttr*)0x10, (const KernelSchedParam*)0x10); return 0; }
    case 75: { std::fprintf(stderr, "pthread_attr_setschedpolicy\n"); (void)pthread_attr_setschedpolicy_nid_postfix((PthreadAttr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 76: { std::fprintf(stderr, "pthread_attr_setscope\n"); (void)pthread_attr_setscope_nid_postfix((PthreadAttr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 77: { std::fprintf(stderr, "pthread_attr_setstacksize\n"); (void)pthread_attr_setstacksize_nid_postfix((PthreadAttr*)0x10, (size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 78: { std::fprintf(stderr, "pthread_attr_setstack\n"); (void)pthread_attr_setstack_nid_postfix((PthreadAttr*)0x10, (void*)0x10, (size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 79: { std::fprintf(stderr, "pthread_attr_setsolosched_np\n"); (void)pthread_attr_setsolosched_np_nid_postfix((PthreadAttr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 80: { std::fprintf(stderr, "pthread_barrierattr_init\n"); (void)pthread_barrierattr_init_nid_postfix((PthreadBarrierattrPrivate**)0x10); return 0; }
    case 81: { std::fprintf(stderr, "pthread_barrierattr_destroy\n"); (void)pthread_barrierattr_destroy_nid_postfix((PthreadBarrierattrPrivate**)0x10); return 0; }
    case 82: { std::fprintf(stderr, "pthread_barrierattr_getpshared\n"); (void)pthread_barrierattr_getpshared_nid_postfix((PthreadBarrierattrPrivate* const*)0x10, (int*)0x10); return 0; }
    case 83: { std::fprintf(stderr, "pthread_barrierattr_setpshared\n"); (void)pthread_barrierattr_setpshared_nid_postfix((PthreadBarrierattrPrivate**)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 84: { std::fprintf(stderr, "pthread_barrier_init\n"); (void)pthread_barrier_init_nid_postfix((PthreadBarrierPrivate**)0x10, (PthreadBarrierattrPrivate* const*)0x10, (unsigned)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 85: { std::fprintf(stderr, "pthread_barrier_wait\n"); (void)pthread_barrier_wait_nid_postfix((PthreadBarrierPrivate**)0x10); return 0; }
    case 86: { std::fprintf(stderr, "pthread_barrier_destroy\n"); (void)pthread_barrier_destroy_nid_postfix((PthreadBarrierPrivate**)0x10); return 0; }
    case 87: { std::fprintf(stderr, "pthread_barrier_setname_np\n"); (void)pthread_barrier_setname_np_nid_postfix((PthreadBarrierPrivate**)0x10, (const char*)0x10); return 0; }
    case 88: { std::fprintf(stderr, "pthread_cond_destroy\n"); (void)pthread_cond_destroy_nid_postfix((PthreadCond*)0x10); return 0; }
    case 89: { std::fprintf(stderr, "pthread_cond_broadcast\n"); (void)pthread_cond_broadcast_nid_postfix((PthreadCond*)0x10); return 0; }
    case 90: { std::fprintf(stderr, "pthread_cond_init\n"); (void)pthread_cond_init_nid_postfix((PthreadCond*)0x10, (const PthreadCondattr*)0x10); return 0; }
    case 91: { std::fprintf(stderr, "pthread_cond_signal\n"); (void)pthread_cond_signal_nid_postfix((PthreadCond*)0x10); return 0; }
    case 92: { std::fprintf(stderr, "pthread_cond_timedwait\n"); (void)pthread_cond_timedwait_nid_postfix((PthreadCond*)0x10, (PthreadMutex*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 93: { std::fprintf(stderr, "pthread_cond_reltimedwait_np\n"); (void)pthread_cond_reltimedwait_np_nid_postfix((PthreadCond*)0x10, (PthreadMutex*)0x10, (KernelUseconds)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 94: { std::fprintf(stderr, "pthread_cond_wait\n"); (void)pthread_cond_wait_nid_postfix((PthreadCond*)0x10, (PthreadMutex*)0x10); return 0; }
    case 95: { std::fprintf(stderr, "pthread_cond_setname_np\n"); (void)pthread_cond_setname_np_nid_postfix((PthreadCond*)0x10, (const char*)0x10); return 0; }
    case 96: { std::fprintf(stderr, "pthread_condattr_destroy\n"); (void)pthread_condattr_destroy_nid_postfix((PthreadCondattr*)0x10); return 0; }
    case 97: { std::fprintf(stderr, "pthread_condattr_init\n"); (void)pthread_condattr_init_nid_postfix((PthreadCondattr*)0x10); return 0; }
    case 98: { std::fprintf(stderr, "pthread_condattr_setclock\n"); (void)pthread_condattr_setclock_nid_postfix((PthreadCondattr*)0x10, (KernelClockid)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 99: { std::fprintf(stderr, "pthread_condattr_getclock\n"); (void)pthread_condattr_getclock_nid_postfix((const PthreadCondattr*)0x10, (KernelClockid*)0x10); return 0; }
    case 100: { std::fprintf(stderr, "pthread_condattr_setpshared\n"); (void)pthread_condattr_setpshared_nid_postfix((PthreadCondattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 101: { std::fprintf(stderr, "pthread_condattr_getpshared\n"); (void)pthread_condattr_getpshared_nid_postfix((const PthreadCondattr*)0x10, (int*)0x10); return 0; }
    case 102: { std::fprintf(stderr, "pthread_mutex_destroy\n"); (void)pthread_mutex_destroy_nid_postfix((PthreadMutex*)0x10); return 0; }
    case 103: { std::fprintf(stderr, "pthread_mutex_init\n"); (void)pthread_mutex_init_nid_postfix((PthreadMutex*)0x10, (const PthreadMutexattr*)0x10); return 0; }
    case 104: { std::fprintf(stderr, "pthread_mutex_lock\n"); (void)pthread_mutex_lock_nid_postfix((PthreadMutex*)0x10); return 0; }
    case 105: { std::fprintf(stderr, "pthread_mutex_timedlock\n"); (void)pthread_mutex_timedlock_nid_postfix((PthreadMutex*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 106: { std::fprintf(stderr, "pthread_mutex_trylock\n"); (void)pthread_mutex_trylock_nid_postfix((PthreadMutex*)0x10); return 0; }
    case 107: { std::fprintf(stderr, "pthread_mutex_unlock\n"); (void)pthread_mutex_unlock_nid_postfix((PthreadMutex*)0x10); return 0; }
    case 108: { std::fprintf(stderr, "pthread_mutex_reltimedlock_np\n"); (void)pthread_mutex_reltimedlock_np_nid_postfix((PthreadMutex*)0x10, (KernelUseconds)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 109: { std::fprintf(stderr, "pthread_mutex_isowned_np\n"); (void)pthread_mutex_isowned_np_nid_postfix((PthreadMutex*)0x10); return 0; }
    case 110: { std::fprintf(stderr, "pthread_mutex_getspinloops_np\n"); (void)pthread_mutex_getspinloops_np_nid_postfix((PthreadMutex*)0x10, (int*)0x10); return 0; }
    case 111: { std::fprintf(stderr, "pthread_mutex_setspinloops_np\n"); (void)pthread_mutex_setspinloops_np_nid_postfix((PthreadMutex*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 112: { std::fprintf(stderr, "pthread_mutex_getyieldloops_np\n"); (void)pthread_mutex_getyieldloops_np_nid_postfix((PthreadMutex*)0x10, (int*)0x10); return 0; }
    case 113: { std::fprintf(stderr, "pthread_mutex_setyieldloops_np\n"); (void)pthread_mutex_setyieldloops_np_nid_postfix((PthreadMutex*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 114: { std::fprintf(stderr, "pthread_mutex_getprioceiling\n"); (void)pthread_mutex_getprioceiling_nid_postfix((const PthreadMutex*)0x10, (int*)0x10); return 0; }
    case 115: { std::fprintf(stderr, "pthread_mutex_setprioceiling\n"); (void)pthread_mutex_setprioceiling_nid_postfix((PthreadMutex*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 116: { std::fprintf(stderr, "pthread_mutex_setname_np\n"); (void)pthread_mutex_setname_np_nid_postfix((PthreadMutex*)0x10, (const char*)0x10); return 0; }
    case 117: { std::fprintf(stderr, "pthread_mutexattr_destroy\n"); (void)pthread_mutexattr_destroy_nid_postfix((PthreadMutexattr*)0x10); return 0; }
    case 118: { std::fprintf(stderr, "pthread_mutexattr_init\n"); (void)pthread_mutexattr_init_nid_postfix((PthreadMutexattr*)0x10); return 0; }
    case 119: { std::fprintf(stderr, "pthread_mutexattr_getpshared\n"); (void)pthread_mutexattr_getpshared_nid_postfix((const PthreadMutexattr*)0x10, (int*)0x10); return 0; }
    case 120: { std::fprintf(stderr, "pthread_mutexattr_setpshared\n"); (void)pthread_mutexattr_setpshared_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 121: { std::fprintf(stderr, "pthread_mutexattr_setprotocol\n"); (void)pthread_mutexattr_setprotocol_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 122: { std::fprintf(stderr, "pthread_mutexattr_getprotocol\n"); (void)pthread_mutexattr_getprotocol_nid_postfix((const PthreadMutexattr*)0x10, (int*)0x10); return 0; }
    case 123: { std::fprintf(stderr, "pthread_mutexattr_setprioceiling\n"); (void)pthread_mutexattr_setprioceiling_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 124: { std::fprintf(stderr, "pthread_mutexattr_getprioceiling\n"); (void)pthread_mutexattr_getprioceiling_nid_postfix((const PthreadMutexattr*)0x10, (int*)0x10); return 0; }
    case 125: { std::fprintf(stderr, "pthread_mutexattr_settype\n"); (void)pthread_mutexattr_settype_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 126: { std::fprintf(stderr, "pthread_mutexattr_gettype\n"); (void)pthread_mutexattr_gettype_nid_postfix((const PthreadMutexattr*)0x10, (int*)0x10); return 0; }
    case 127: { std::fprintf(stderr, "pthread_rwlock_destroy\n"); (void)pthread_rwlock_destroy_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 128: { std::fprintf(stderr, "pthread_rwlock_init\n"); (void)pthread_rwlock_init_nid_postfix((PthreadRwlock*)0x10, (const PthreadRwlockattr*)0x10); return 0; }
    case 129: { std::fprintf(stderr, "pthread_rwlockattr_destroy\n"); (void)pthread_rwlockattr_destroy_nid_postfix((PthreadRwlockattr*)0x10); return 0; }
    case 130: { std::fprintf(stderr, "pthread_rwlockattr_init\n"); (void)pthread_rwlockattr_init_nid_postfix((PthreadRwlockattr*)0x10); return 0; }
    case 131: { std::fprintf(stderr, "pthread_rwlockattr_getpshared\n"); (void)pthread_rwlockattr_getpshared_nid_postfix((const PthreadRwlockattr*)0x10, (int*)0x10); return 0; }
    case 132: { std::fprintf(stderr, "pthread_rwlockattr_setpshared\n"); (void)pthread_rwlockattr_setpshared_nid_postfix((PthreadRwlockattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 133: { std::fprintf(stderr, "pthread_rwlock_rdlock\n"); (void)pthread_rwlock_rdlock_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 134: { std::fprintf(stderr, "pthread_rwlock_timedrdlock\n"); (void)pthread_rwlock_timedrdlock_nid_postfix((PthreadRwlock*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 135: { std::fprintf(stderr, "pthread_rwlock_timedwrlock\n"); (void)pthread_rwlock_timedwrlock_nid_postfix((PthreadRwlock*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 136: { std::fprintf(stderr, "pthread_rwlock_reltimedrdlock_np\n"); (void)pthread_rwlock_reltimedrdlock_np_nid_postfix((PthreadRwlock*)0x10, (KernelUseconds)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 137: { std::fprintf(stderr, "pthread_rwlock_reltimedwrlock_np\n"); (void)pthread_rwlock_reltimedwrlock_np_nid_postfix((PthreadRwlock*)0x10, (KernelUseconds)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 138: { std::fprintf(stderr, "pthread_rwlock_tryrdlock\n"); (void)pthread_rwlock_tryrdlock_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 139: { std::fprintf(stderr, "pthread_rwlock_trywrlock\n"); (void)pthread_rwlock_trywrlock_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 140: { std::fprintf(stderr, "pthread_rwlock_unlock\n"); (void)pthread_rwlock_unlock_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 141: { std::fprintf(stderr, "pthread_rwlock_wrlock\n"); (void)pthread_rwlock_wrlock_nid_postfix((PthreadRwlock*)0x10); return 0; }
    case 142: { std::fprintf(stderr, "pthread_rwlock_setname_np\n"); (void)pthread_rwlock_setname_np_nid_postfix((PthreadRwlock*)0x10, (const char*)0x10); return 0; }
    case 143: { std::fprintf(stderr, "ksem_init\n"); (void)ksem_init_nid_postfix((std::intptr_t*)0x10, (unsigned int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 144: { std::fprintf(stderr, "ksem_timedwait\n"); (void)ksem_timedwait_nid_postfix((std::intptr_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelTimespec*)0x10); return 0; }
    case 145: { std::fprintf(stderr, "ksem_getvalue\n"); (void)ksem_getvalue_nid_postfix((std::intptr_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 146: { std::fprintf(stderr, "sem_init\n"); (void)sem_init_nid_postfix((void*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (unsigned int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 147: { std::fprintf(stderr, "sem_destroy\n"); (void)sem_destroy_nid_postfix((void*)0x10); return 0; }
    case 148: { std::fprintf(stderr, "sem_wait\n"); (void)sem_wait_nid_postfix((void*)0x10); return 0; }
    case 149: { std::fprintf(stderr, "sem_trywait\n"); (void)sem_trywait_nid_postfix((void*)0x10); return 0; }
    case 150: { std::fprintf(stderr, "sem_timedwait\n"); (void)sem_timedwait_nid_postfix((void*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 151: { std::fprintf(stderr, "sem_reltimedwait_np\n"); (void)sem_reltimedwait_np_nid_postfix((void*)0x10, (uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 152: { std::fprintf(stderr, "sem_post\n"); (void)sem_post_nid_postfix((void*)0x10); return 0; }
    case 153: { std::fprintf(stderr, "sem_getvalue\n"); (void)sem_getvalue_nid_postfix((void*)0x10, (int*)0x10); return 0; }
    case 154: { std::fprintf(stderr, "sem_setname\n"); (void)sem_setname_nid_postfix((void*)0x10, (const char*)0x10); return 0; }
    case 155: { std::fprintf(stderr, "sem_unlink\n"); (void)sem_unlink_nid_postfix((const char*)0x10); return 0; }
    case 156: { std::fprintf(stderr, "pthread_spin_init\n"); (void)pthread_spin_init_nid_postfix((PthreadSpinlockPrivate**)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 157: { std::fprintf(stderr, "pthread_spin_destroy\n"); (void)pthread_spin_destroy_nid_postfix((PthreadSpinlockPrivate**)0x10); return 0; }
    case 158: { std::fprintf(stderr, "pthread_spin_trylock\n"); (void)pthread_spin_trylock_nid_postfix((PthreadSpinlockPrivate**)0x10); return 0; }
    case 159: { std::fprintf(stderr, "pthread_spin_lock\n"); (void)pthread_spin_lock_nid_postfix((PthreadSpinlockPrivate**)0x10); return 0; }
    case 160: { std::fprintf(stderr, "pthread_spin_unlock\n"); (void)pthread_spin_unlock_nid_postfix((PthreadSpinlockPrivate**)0x10); return 0; }
    case 161: { std::fprintf(stderr, "cpuset_getaffinity\n"); (void)cpuset_getaffinity_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 162: { std::fprintf(stderr, "cpuset_setaffinity\n"); (void)cpuset_setaffinity_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10); return 0; }
    case 163: { std::fprintf(stderr, "cpuset\n"); (void)cpuset_nid_postfix((int*)0x10); return 0; }
    case 164: { std::fprintf(stderr, "cpuset_getid\n"); (void)cpuset_getid_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 165: { std::fprintf(stderr, "rtprio_thread\n"); (void)rtprio_thread_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestRtPrio*)0x10); return 0; }
    case 166: { std::fprintf(stderr, "pthread_create\n"); (void)pthread_create_nid_postfix((Pthread*)0x10, (const PthreadAttr*)0x10, (PthreadEntry)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 167: { std::fprintf(stderr, "pthread_create_name_np\n"); (void)pthread_create_name_np_nid_postfix((Pthread*)0x10, (const PthreadAttr*)0x10, (PthreadEntry)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (const char*)0x10); return 0; }
    case 168: { std::fprintf(stderr, "pthread_getschedparam\n"); (void)pthread_getschedparam_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10, (KernelSchedParam*)0x10); return 0; }
    case 169: { std::fprintf(stderr, "pthread_getaffinity_np\n"); (void)pthread_getaffinity_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 170: { std::fprintf(stderr, "pthread_setaffinity_np\n"); (void)pthread_setaffinity_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10); return 0; }
    case 171: { std::fprintf(stderr, "pthread_join\n"); (void)pthread_join_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void**)0x10); return 0; }
    case 172: { std::fprintf(stderr, "pthread_timedjoin_np\n"); (void)pthread_timedjoin_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void**)0x10, (const KernelTimespec*)0x10); return 0; }
    case 173: { std::fprintf(stderr, "pthread_rename_np\n"); (void)pthread_rename_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10); return 0; }
    case 174: { std::fprintf(stderr, "pthread_getname_np\n"); (void)pthread_getname_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (char*)0x10); return 0; }
    case 175: { std::fprintf(stderr, "pthread_set_name_np\n"); (void)pthread_set_name_np_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10); return 0; }
    case 176: { std::fprintf(stderr, "pthread_getcpuclockid\n"); (void)pthread_getcpuclockid_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 177: { std::fprintf(stderr, "pthread_setcancelstate\n"); (void)pthread_setcancelstate_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 178: { std::fprintf(stderr, "pthread_setcanceltype\n"); (void)pthread_setcanceltype_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 179: { std::fprintf(stderr, "pthread_setschedparam\n"); (void)pthread_setschedparam_nid_postfix((Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelSchedParam*)0x10); return 0; }
    case 180: { std::fprintf(stderr, "pthread_setspecific\n"); (void)pthread_setspecific_nid_postfix((PthreadKey)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 181: { std::fprintf(stderr, "pthread_key_create\n"); (void)pthread_key_create_nid_postfix((PthreadKey*)0x10, (pthread_key_destructor_func_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 182: { std::fprintf(stderr, "__pthread_cxa_finalize\n"); (void)__pthread_cxa_finalize_nid_postfix((void*)0x10); return 0; }
    case 183: { std::fprintf(stderr, "__inet_pton\n"); (void)__inet_pton_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (void*)0x10); return 0; }
    case 184: { std::fprintf(stderr, "__inet_ntop\n"); (void)__inet_ntop_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (char*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 185: { std::fprintf(stderr, "pselect\n"); (void)pselect_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (void*)0x10, (void*)0x10, (const void*)0x10, (const void*)0x10); return 0; }
    case 186: { std::fprintf(stderr, "inet_ntop\n"); (void)inet_ntop_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (char*)0x10, (uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 187: { std::fprintf(stderr, "inet_pton\n"); (void)inet_pton_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (void*)0x10); return 0; }
    case 188: { std::fprintf(stderr, "select\n"); (void)select_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (void*)0x10, (void*)0x10, (const void*)0x10); return 0; }
    case 189: { std::fprintf(stderr, "setsockopt\n"); (void)setsockopt_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 190: { std::fprintf(stderr, "getsockopt\n"); (void)getsockopt_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 191: { std::fprintf(stderr, "__sys_socketex\n"); (void)__sys_socketex_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 192: { std::fprintf(stderr, "socketpair\n"); (void)socketpair_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10); return 0; }
    case 193: { std::fprintf(stderr, "connect\n"); (void)connect_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 194: { std::fprintf(stderr, "accept\n"); (void)accept_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 195: { std::fprintf(stderr, "send\n"); (void)send_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 196: { std::fprintf(stderr, "recv\n"); (void)recv_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 197: { std::fprintf(stderr, "bind\n"); (void)bind_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 198: { std::fprintf(stderr, "getsockname\n"); (void)getsockname_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 199: { std::fprintf(stderr, "getpeername\n"); (void)getpeername_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 200: { std::fprintf(stderr, "ioctl\n"); (void)ioctl_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 201: { std::fprintf(stderr, "_ioctl\n"); (void)_ioctl_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 202: { std::fprintf(stderr, "sendto\n"); (void)sendto_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 203: { std::fprintf(stderr, "recvfrom\n"); (void)recvfrom_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 204: { std::fprintf(stderr, "sendmsg\n"); (void)sendmsg_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestMsghdr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 205: { std::fprintf(stderr, "recvmsg\n"); (void)recvmsg_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestMsghdr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 206: { std::fprintf(stderr, "sendfile\n"); (void)sendfile_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSendfileHeaders*)0x10, (std::int64_t*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 207: { std::fprintf(stderr, "poll\n"); (void)poll_nid_postfix((GuestPollDescriptor*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 208: { std::fprintf(stderr, "_setsockopt\n"); (void)_setsockopt_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 209: { std::fprintf(stderr, "_getsockopt\n"); (void)_getsockopt_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 210: { std::fprintf(stderr, "_connect\n"); (void)_connect_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 211: { std::fprintf(stderr, "_accept\n"); (void)_accept_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 212: { std::fprintf(stderr, "_bind\n"); (void)_bind_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 213: { std::fprintf(stderr, "_getsockname\n"); (void)_getsockname_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 214: { std::fprintf(stderr, "_getpeername\n"); (void)_getpeername_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 215: { std::fprintf(stderr, "_sendto\n"); (void)_sendto_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 216: { std::fprintf(stderr, "_recvfrom\n"); (void)_recvfrom_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::uint32_t*)0x10); return 0; }
    case 217: { std::fprintf(stderr, "_sendmsg\n"); (void)_sendmsg_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestMsghdr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 218: { std::fprintf(stderr, "_recvmsg\n"); (void)_recvmsg_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestMsghdr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 219: { std::fprintf(stderr, "_umtx_op\n"); (void)_umtx_op_nid_postfix((void*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (void*)0x10); return 0; }
    case 220: { std::fprintf(stderr, "getlogin_r\n"); (void)getlogin_r_nid_postfix((char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 221: { std::fprintf(stderr, "getresuid\n"); (void)getresuid_nid_postfix((std::uint32_t*)0x10, (std::uint32_t*)0x10, (std::uint32_t*)0x10); return 0; }
    case 222: { std::fprintf(stderr, "getresgid\n"); (void)getresgid_nid_postfix((std::uint32_t*)0x10, (std::uint32_t*)0x10, (std::uint32_t*)0x10); return 0; }
    case 223: { std::fprintf(stderr, "waitpid\n"); (void)waitpid_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 224: { std::fprintf(stderr, "setrlimit\n"); (void)setrlimit_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestResourceLimit*)0x10); return 0; }
    case 225: { std::fprintf(stderr, "wait\n"); (void)wait_nid_postfix((int*)0x10); return 0; }
    case 226: { std::fprintf(stderr, "sched_getparam\n"); (void)sched_getparam_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelSchedParam*)0x10); return 0; }
    case 227: { std::fprintf(stderr, "sched_setparam\n"); (void)sched_setparam_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelSchedParam*)0x10); return 0; }
    case 228: { std::fprintf(stderr, "getrusage\n"); (void)getrusage_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestResourceUsage*)0x10); return 0; }
    case 229: { std::fprintf(stderr, "getrlimit\n"); (void)getrlimit_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestResourceLimit*)0x10); return 0; }
    case 230: { std::fprintf(stderr, "setgroups\n"); (void)setgroups_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const std::uint32_t*)0x10); return 0; }
    case 231: { std::fprintf(stderr, "getgroups\n"); (void)getgroups_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint32_t*)0x10); return 0; }
    case 232: { std::fprintf(stderr, "getitimer\n"); (void)getitimer_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestItimerval*)0x10); return 0; }
    case 233: { std::fprintf(stderr, "setitimer\n"); (void)setitimer_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestItimerval*)0x10, (GuestItimerval*)0x10); return 0; }
    case 234: { std::fprintf(stderr, "sigaction\n"); (void)sigaction_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSigaction*)0x10, (GuestSigaction*)0x10); return 0; }
    case 235: { std::fprintf(stderr, "_sigaction\n"); (void)_sigaction_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSigaction*)0x10, (GuestSigaction*)0x10); return 0; }
    case 236: { std::fprintf(stderr, "sigaltstack\n"); (void)sigaltstack_nid_postfix((const GuestStack*)0x10, (GuestStack*)0x10); return 0; }
    case 237: { std::fprintf(stderr, "_sigprocmask\n"); (void)_sigprocmask_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSignalSet*)0x10, (GuestSignalSet*)0x10); return 0; }
    case 238: { std::fprintf(stderr, "sigprocmask\n"); (void)sigprocmask_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (void*)0x10); return 0; }
    case 239: { std::fprintf(stderr, "pthread_sigmask\n"); (void)pthread_sigmask_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSignalSet*)0x10, (GuestSignalSet*)0x10); return 0; }
    case 240: { std::fprintf(stderr, "sigtimedwait\n"); (void)sigtimedwait_nid_postfix((const GuestSignalSet*)0x10, (void*)0x10, (const KernelTimespec*)0x10); return 0; }
    case 241: { std::fprintf(stderr, "sigwaitinfo\n"); (void)sigwaitinfo_nid_postfix((const GuestSignalSet*)0x10, (void*)0x10); return 0; }
    case 242: { std::fprintf(stderr, "sigwait\n"); (void)sigwait_nid_postfix((const GuestSignalSet*)0x10, (int*)0x10); return 0; }
    case 243: { std::fprintf(stderr, "sigpending\n"); (void)sigpending_nid_postfix((GuestSignalSet*)0x10); return 0; }
    case 244: { std::fprintf(stderr, "sigemptyset\n"); (void)sigemptyset_nid_postfix((GuestSignalSet*)0x10); return 0; }
    case 245: { std::fprintf(stderr, "sigfillset\n"); (void)sigfillset_nid_postfix((GuestSignalSet*)0x10); return 0; }
    case 246: { std::fprintf(stderr, "sigaddset\n"); (void)sigaddset_nid_postfix((GuestSignalSet*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 247: { std::fprintf(stderr, "sigdelset\n"); (void)sigdelset_nid_postfix((GuestSignalSet*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 248: { std::fprintf(stderr, "sigismember\n"); (void)sigismember_nid_postfix((const GuestSignalSet*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 249: { std::fprintf(stderr, "sceKernelGetProsperoSystemSwVersion\n"); (void)sceKernelGetProsperoSystemSwVersion_nid_postfix((SceKernelProsperoSystemSwVersion*)0x10); return 0; }
    case 250: { std::fprintf(stderr, "sceKernelGetAllowedSdkVersionOnSystem\n"); (void)sceKernelGetAllowedSdkVersionOnSystem_nid_postfix((std::uint32_t*)0x10); return 0; }
    case 251: { std::fprintf(stderr, "sysctl\n"); (void)sysctl_nid_postfix((const int*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10, (std::size_t*)0x10, (const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 252: { std::fprintf(stderr, "sysctlbyname\n"); (void)sysctlbyname_nid_postfix((const char*)0x10, (void*)0x10, (std::size_t*)0x10, (const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 253: { std::fprintf(stderr, "sceKernelGetSystemSwVersion\n"); (void)sceKernelGetSystemSwVersion_nid_postfix((SceKernelSystemSwVersion*)0x10); return 0; }
    case 254: { std::fprintf(stderr, "sysctlnametomib\n"); (void)sysctlnametomib_nid_postfix((const char*)0x10, (int*)0x10, (std::size_t*)0x10); return 0; }
    case 255: { std::fprintf(stderr, "ktimer_create\n"); (void)ktimer_create_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestSigevent*)0x10, (int*)0x10); return 0; }
    case 256: { std::fprintf(stderr, "ktimer_settime\n"); (void)ktimer_settime_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const GuestItimerspec*)0x10, (GuestItimerspec*)0x10); return 0; }
    case 257: { std::fprintf(stderr, "ktimer_gettime\n"); (void)ktimer_gettime_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestItimerspec*)0x10); return 0; }
    case 258: { std::fprintf(stderr, "_nanosleep\n"); (void)_nanosleep_nid_postfix((const KernelTimespec*)0x10, (KernelTimespec*)0x10); return 0; }
    case 259: { std::fprintf(stderr, "clock_settime\n"); (void)clock_settime_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelTimespec*)0x10); return 0; }
    case 260: { std::fprintf(stderr, "pthread_cond_signalto_np\n"); (void)pthread_cond_signalto_np_nid_postfix((PthreadCond*)0x10, (Pthread)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 261: { std::fprintf(stderr, "pthread_mutexattr_setkind_np\n"); (void)pthread_mutexattr_setkind_np_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 262: { std::fprintf(stderr, "pthread_rwlockattr_gettype_np\n"); (void)pthread_rwlockattr_gettype_np_nid_postfix((const PthreadRwlockattr*)0x10, (int*)0x10); return 0; }
    case 263: { std::fprintf(stderr, "pthread_rwlockattr_settype_np\n"); (void)pthread_rwlockattr_settype_np_nid_postfix((PthreadRwlockattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 264: { std::fprintf(stderr, "sysKernelGetLowerLimitSysexVersion\n"); (void)sysKernelGetLowerLimitSysexVersion_nid_postfix((char*)0x10); return 0; }
    case 265: { std::fprintf(stderr, "sysKernelGetLowerLimitUpdVersion\n"); (void)sysKernelGetLowerLimitUpdVersion_nid_postfix((std::uint32_t*)0x10); return 0; }
    case 266: { std::fprintf(stderr, "sysKernelGetUpdVersion\n"); (void)sysKernelGetUpdVersion_nid_postfix((std::uint32_t*)0x10); return 0; }
    case 267: { std::fprintf(stderr, "aio_read\n"); (void)aio_read_nid_postfix((GuestAiocb*)0x10); return 0; }
    case 268: { std::fprintf(stderr, "aio_write\n"); (void)aio_write_nid_postfix((GuestAiocb*)0x10); return 0; }
    case 269: { std::fprintf(stderr, "aio_fsync\n"); (void)aio_fsync_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestAiocb*)0x10); return 0; }
    case 270: { std::fprintf(stderr, "aio_error\n"); (void)aio_error_nid_postfix((const GuestAiocb*)0x10); return 0; }
    case 271: { std::fprintf(stderr, "aio_return\n"); (void)aio_return_nid_postfix((GuestAiocb*)0x10); return 0; }
    case 272: { std::fprintf(stderr, "aio_suspend\n"); (void)aio_suspend_nid_postfix((const GuestAiocb* const*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelTimespec*)0x10); return 0; }
    case 273: { std::fprintf(stderr, "aio_waitcomplete\n"); (void)aio_waitcomplete_nid_postfix((GuestAiocb**)0x10, (const KernelTimespec*)0x10); return 0; }
    case 274: { std::fprintf(stderr, "aio_cancel\n"); (void)aio_cancel_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (GuestAiocb*)0x10); return 0; }
    case 275: { std::fprintf(stderr, "fstatfs\n"); (void)fstatfs_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelStatfs*)0x10); return 0; }
    case 276: { std::fprintf(stderr, "fstat\n"); (void)fstat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (FileStat*)0x10); return 0; }
    case 277: { std::fprintf(stderr, "readlink\n"); (void)readlink_nid_postfix((const char*)0x10, (char*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 278: { std::fprintf(stderr, "readlinkat\n"); (void)readlinkat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (char*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 279: { std::fprintf(stderr, "pathconf\n"); (void)pathconf_nid_postfix((const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 280: { std::fprintf(stderr, "link\n"); (void)link_nid_postfix((const char*)0x10, (const char*)0x10); return 0; }
    case 281: { std::fprintf(stderr, "linkat\n"); (void)linkat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 282: { std::fprintf(stderr, "symlink\n"); (void)symlink_nid_postfix((const char*)0x10, (const char*)0x10); return 0; }
    case 283: { std::fprintf(stderr, "mkdirat\n"); (void)mkdirat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (std::uint16_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 284: { std::fprintf(stderr, "fchmodat\n"); (void)fchmodat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 285: { std::fprintf(stderr, "lchmod\n"); (void)lchmod_nid_postfix((const char*)0x10, (std::uint16_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 286: { std::fprintf(stderr, "lchflags\n"); (void)lchflags_nid_postfix((const char*)0x10, (std::uint64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 287: { std::fprintf(stderr, "sched_rr_get_interval\n"); (void)sched_rr_get_interval_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelTimespec*)0x10); return 0; }
    case 288: { std::fprintf(stderr, "chown\n"); (void)chown_nid_postfix((const char*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 289: { std::fprintf(stderr, "lchown\n"); (void)lchown_nid_postfix((const char*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 290: { std::fprintf(stderr, "fchownat\n"); (void)fchownat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::uint32_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 291: { std::fprintf(stderr, "fstatat\n"); (void)fstatat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (FileStat*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 292: { std::fprintf(stderr, "settimeofday\n"); (void)settimeofday_nid_postfix((const KernelTimeval*)0x10, (const KernelTimezone*)0x10); return 0; }
    case 293: { std::fprintf(stderr, "unlinkat\n"); (void)unlinkat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 294: { std::fprintf(stderr, "renameat\n"); (void)renameat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10); return 0; }
    case 295: { std::fprintf(stderr, "get_authinfo\n"); (void)get_authinfo_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (KernelAuthInfo*)0x10); return 0; }
    case 296: { std::fprintf(stderr, "getfsstat\n"); (void)getfsstat_nid_postfix((KernelStatfs*)0x10, (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 297: { std::fprintf(stderr, "kenv\n"); (void)kenv_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const char*)0x10, (char*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 298: { std::fprintf(stderr, "kldfind\n"); (void)kldfind_nid_postfix((const char*)0x10); return 0; }
    case 299: { std::fprintf(stderr, "kldload\n"); (void)kldload_nid_postfix((const char*)0x10); return 0; }
    case 300: { std::fprintf(stderr, "kldstat\n"); (void)kldstat_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 301: { std::fprintf(stderr, "kldsym\n"); (void)kldsym_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (void*)0x10); return 0; }
    case 302: { std::fprintf(stderr, "pthread_attr_getsolosched_np\n"); (void)pthread_attr_getsolosched_np_nid_postfix((const PthreadAttr*)0x10, (int*)0x10); return 0; }
    case 303: { std::fprintf(stderr, "pthread_attr_setcreatesuspend_np\n"); (void)pthread_attr_setcreatesuspend_np_nid_postfix((PthreadAttr*)0x10); return 0; }
    case 304: { std::fprintf(stderr, "pthread_attr_setstackaddr\n"); (void)pthread_attr_setstackaddr_nid_postfix((PthreadAttr*)0x10, (void*)0x10); return 0; }
    case 305: { std::fprintf(stderr, "pthread_mutexattr_getgen_np\n"); (void)pthread_mutexattr_getgen_np_nid_postfix((const PthreadMutexattr*)0x10, (int*)0x10); return 0; }
    case 306: { std::fprintf(stderr, "pthread_mutexattr_setgen_np\n"); (void)pthread_mutexattr_setgen_np_nid_postfix((PthreadMutexattr*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 307: { std::fprintf(stderr, "pwrite\n"); (void)pwrite_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (std::int64_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 308: { std::fprintf(stderr, "revoke\n"); (void)revoke_nid_postfix((const char*)0x10); return 0; }
    case 309: { std::fprintf(stderr, "sched_setscheduler\n"); (void)sched_setscheduler_nid_postfix((int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16), (const KernelSchedParam*)0x10); return 0; }
    case 310: { std::fprintf(stderr, "setlogin\n"); (void)setlogin_nid_postfix((const char*)0x10); return 0; }
    case 311: { std::fprintf(stderr, "sigreturn\n"); (void)sigreturn_nid_postfix((const UserUcontext*)0x10); return 0; }
    case 312: { std::fprintf(stderr, "utrace\n"); (void)utrace_nid_postfix((const void*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 313: { std::fprintf(stderr, "uuidgen\n"); (void)uuidgen_nid_postfix((GuestUuid*)0x10, (int)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    case 314: { std::fprintf(stderr, "__getcwd\n"); (void)__getcwd_nid_postfix((char*)0x10, (std::size_t)(variant == 0 ? 0 : variant == 1 ? 1 : 16)); return 0; }
    default: return 2;
  }
}
int main(int argc, char** argv) {
  if (argc < 3) { std::printf("%d\n", 315); return 0; }
  return Call(std::atoi(argv[1]), std::atoi(argv[2]));
}

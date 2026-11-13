#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceCustomMusicSysCallWrapper/include/InventoryExports.hpp"
#include "prx/libSceCustomMusicSysCallWrapper/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudiodecClearContext(int32_t handle);
int APS5_VABI sceAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type);
int APS5_VABI sceAudiodecCreateDecoder(AudiodecCtrl* ctrl, uint32_t codecType);
int APS5_VABI sceAudiodecDecode(int32_t handle, AudiodecCtrl* ctrl);
int APS5_VABI sceAudiodecDeleteDecoder(int32_t handle);
int APS5_VABI sceKernelClose(int d);
int APS5_VABI sceKernelGettimeofday(KernelTimeval* tp);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
int APS5_VABI sceKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp);
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes);
int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen);
int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen);
int APS5_VABI sceNetConnect(int s, const void* addr, uint32_t addrlen);
int APS5_VABI sceNetEpollAbort(int eid, int flags);
int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event);
int APS5_VABI sceNetEpollCreate(const char* name, int flags);
int APS5_VABI sceNetEpollDestroy(int eid);
int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout);
int* APS5_VABI sceNetErrnoLoc(void);
const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size);
int APS5_VABI sceNetInetPton(int af, const char* src, void* dst);
int APS5_VABI sceNetListen(int s, int backlog);
int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI sceNetPoolDestroy(int memid);
std::int64_t APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags);
int APS5_VABI sceNetResolverAbort(int rid, int flags);
int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags);
int APS5_VABI sceNetResolverDestroy(int rid);
std::int64_t APS5_VABI sceNetSend(int s, const void* buf, size_t len, int flags);
int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen);
int APS5_VABI sceNetShutdown(int s, int how);
int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol);
int APS5_VABI sceNetSocketAbort(int s, int flags);
int APS5_VABI sceNetSocketClose(int s);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
Pthread APS5_VABI scePthreadSelf();
void APS5_VABI scePthreadYield();
int APS5_VABI sceNetCtlInit(void);
void APS5_VABI sceNetCtlTerm(void);
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout);
}

extern "C" {

const char* APS5_VABI sceCustomMusicNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
    return sceNetInetNtop(af, src, dst, size);
}

int APS5_VABI sceCustomMusicNetShutdown(int s, int how) {
    return sceNetShutdown(s, how);
}

int APS5_VABI sceCustomMusicNetSocketAbort(int s, int flags) {
    return sceNetSocketAbort(s, flags);
}

int APS5_VABI sceCustomMusicPthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    return scePthreadAttrGet(thread, attr);
}

Pthread APS5_VABI sceCustomMusicPthreadSelf() {
    return scePthreadSelf();
}

int APS5_VABI sceCustomMusicSelect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    return select_nid_postfix(nfds, readfds, writefds, exceptfds, timeout);
}

}

extern "C" {

int APS5_VABI sceCustomMusicAudiodecClearContext(int32_t handle) {
    return sceAudiodecClearContext(handle);
}

int APS5_VABI sceCustomMusicAudiodecCpuDecode(const AudiodecCtrl* ctrl, const AudiodecCpuMemory* memory, std::uint32_t type) {
    return sceAudiodecCpuDecode(ctrl, memory, type);
}

int APS5_VABI sceCustomMusicAudiodecCreateDecoder(AudiodecCtrl* ctrl, uint32_t codecType) {
    return sceAudiodecCreateDecoder(ctrl, codecType);
}

int APS5_VABI sceCustomMusicAudiodecDecode(int32_t handle, AudiodecCtrl* ctrl) {
    return sceAudiodecDecode(handle, ctrl);
}

int APS5_VABI sceCustomMusicAudiodecDeleteDecoder(int32_t handle) {
    return sceAudiodecDeleteDecoder(handle);
}

int APS5_VABI sceCustomMusicFileClose(int d) {
    return sceKernelClose(d);
}

std::int64_t APS5_VABI sceCustomMusicFileLseek(int d, std::int64_t offset, int whence) {
    return sceKernelLseek(d, offset, whence);
}

int APS5_VABI sceCustomMusicFileOpenRead(const char* path, int, std::uint16_t) {
    return sceKernelOpen(path, 0, 0555);
}

std::int64_t APS5_VABI sceCustomMusicFileRead(int d, void* buf, std::size_t nbytes) {
    return sceKernelRead(d, buf, nbytes);
}

int APS5_VABI sceCustomMusicKernelGettimeofday(KernelTimeval* tp) {
    return sceKernelGettimeofday(tp);
}

int APS5_VABI sceCustomMusicKernelNanosleep(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

int APS5_VABI sceCustomMusicNetAccept(int s, void* addr, uint32_t* addrlen) {
    return sceNetAccept(s, addr, addrlen);
}

int APS5_VABI sceCustomMusicNetBind(int s, const void* addr, uint32_t addrlen) {
    return sceNetBind_nid_postfix(s, addr, addrlen);
}

int APS5_VABI sceCustomMusicNetConnect(int s, const void* addr, uint32_t addrlen) {
    return sceNetConnect(s, addr, addrlen);
}

int APS5_VABI sceCustomMusicNetCtlInit() {
    return sceNetCtlInit();
}

void APS5_VABI sceCustomMusicNetCtlTerm() {
    sceNetCtlTerm();
}

int APS5_VABI sceCustomMusicNetEpollAbort(int eid, int flags) {
    return sceNetEpollAbort(eid, flags);
}

int APS5_VABI sceCustomMusicNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
    return sceNetEpollControl(eid, op, id, event);
}

int APS5_VABI sceCustomMusicNetEpollCreate(const char* name, int flags) {
    return sceNetEpollCreate(name, flags);
}

int APS5_VABI sceCustomMusicNetEpollDestroy(int eid) {
    return sceNetEpollDestroy(eid);
}

int APS5_VABI sceCustomMusicNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
    return sceNetEpollWait(eid, events, maxevents, timeout);
}

int* APS5_VABI sceCustomMusicNetErrnoLoc() {
    return sceNetErrnoLoc();
}

int APS5_VABI sceCustomMusicNetInetPton(int af, const char* src, void* dst) {
    return sceNetInetPton(af, src, dst);
}

int APS5_VABI sceCustomMusicNetListen(int s, int backlog) {
    return sceNetListen(s, backlog);
}

int APS5_VABI sceCustomMusicNetPoolCreate(const char* name, int size, int flags) {
    return sceNetPoolCreate(name, size, flags);
}

int APS5_VABI sceCustomMusicNetPoolDestroy(int memid) {
    return sceNetPoolDestroy(memid);
}

std::int64_t APS5_VABI sceCustomMusicNetRecv(int s, void* buf, size_t len, int flags) {
    return sceNetRecv(s, buf, len, flags);
}

int APS5_VABI sceCustomMusicNetResolverAbort(int rid, int flags) {
    return sceNetResolverAbort(rid, flags);
}

int APS5_VABI sceCustomMusicNetResolverCreate(const char* name, int memid, int flags) {
    return sceNetResolverCreate(name, memid, flags);
}

int APS5_VABI sceCustomMusicNetResolverDestroy(int rid) {
    return sceNetResolverDestroy(rid);
}

std::int64_t APS5_VABI sceCustomMusicNetSend(int s, const void* buf, size_t len, int flags) {
    return sceNetSend(s, buf, len, flags);
}

int APS5_VABI sceCustomMusicNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    return sceNetSetsockopt(s, level, optname, optval, optlen);
}

int APS5_VABI sceCustomMusicNetSocketClose(int s) {
    return sceNetSocketClose(s);
}

int APS5_VABI sceCustomMusicNetSocket(const char* name, int family, int type, int protocol) {
    return sceNetSocket(name, family, type, protocol);
}

int APS5_VABI sceCustomMusicPthreadAttrDestroy(PthreadAttr* attr) {
    return scePthreadAttrDestroy(attr);
}

int APS5_VABI sceCustomMusicPthreadAttrInit(PthreadAttr* attr) {
    return scePthreadAttrInit(attr);
}

int APS5_VABI sceCustomMusicPthreadCondDestroy(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI sceCustomMusicPthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
    return scePthreadCondInit(cond, attr, name);
}

int APS5_VABI sceCustomMusicPthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI sceCustomMusicPthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return scePthreadCreate(thread, attr, entry, arg, name);
}

int APS5_VABI sceCustomMusicPthreadDetach(Pthread thread) {
    return scePthreadDetach(thread);
}

void APS5_VABI sceCustomMusicPthreadExit(void* retval) {
    scePthreadExit(retval);
    std::abort();
}

int APS5_VABI sceCustomMusicPthreadJoin(Pthread thread, void** retval) {
    return scePthreadJoin(thread, retval);
}

int APS5_VABI sceCustomMusicPthreadMutexDestroy(PthreadMutex* mutex) {
    return scePthreadMutexDestroy(mutex);
}

int APS5_VABI sceCustomMusicPthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
    return scePthreadMutexInit(mutex, attr, name);
}

int APS5_VABI sceCustomMusicPthreadMutexLock(PthreadMutex* mutex) {
    return scePthreadMutexLock(mutex);
}

int APS5_VABI sceCustomMusicPthreadSetprio(Pthread thread, int prio) {
    const int error = scePthreadSetprio(thread, prio + 254);
    return error < 0 ? error : 0;
}

void APS5_VABI sceCustomMusicPthreadYield() {
    scePthreadYield();
}

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown__plus_LdekyVYugM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+LdekyVYugM", libSceCustomMusicSysCallWrapperUnknown__plus_LdekyVYugM_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown__minus_f8PcMdF8L0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-f8PcMdF8L0", libSceCustomMusicSysCallWrapperUnknown__minus_f8PcMdF8L0_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_0WBrHbI33Ec_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0WBrHbI33Ec", libSceCustomMusicSysCallWrapperUnknown_0WBrHbI33Ec_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_0d4Aw8u3xzE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0d4Aw8u3xzE", libSceCustomMusicSysCallWrapperUnknown_0d4Aw8u3xzE_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_1auNQHxva3I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1auNQHxva3I", libSceCustomMusicSysCallWrapperUnknown_1auNQHxva3I_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_24ZHuc5b0RA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("24ZHuc5b0RA", libSceCustomMusicSysCallWrapperUnknown_24ZHuc5b0RA_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_2yE7PehS6iI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2yE7PehS6iI", libSceCustomMusicSysCallWrapperUnknown_2yE7PehS6iI_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_3DyCYcJudI4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3DyCYcJudI4", libSceCustomMusicSysCallWrapperUnknown_3DyCYcJudI4_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_5I2IqwB0Hes_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5I2IqwB0Hes", libSceCustomMusicSysCallWrapperUnknown_5I2IqwB0Hes_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_5p7ZpbyUKuo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5p7ZpbyUKuo", libSceCustomMusicSysCallWrapperUnknown_5p7ZpbyUKuo_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_6PNhaGAw6Ho_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6PNhaGAw6Ho", libSceCustomMusicSysCallWrapperUnknown_6PNhaGAw6Ho_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_70NNIr_minus_nfFo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("70NNIr-nfFo", libSceCustomMusicSysCallWrapperUnknown_70NNIr_minus_nfFo_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_7uOVo0lL97g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7uOVo0lL97g", libSceCustomMusicSysCallWrapperUnknown_7uOVo0lL97g_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_8t8UK2XkJ1Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8t8UK2XkJ1Y", libSceCustomMusicSysCallWrapperUnknown_8t8UK2XkJ1Y_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_CQv51NIK3i4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CQv51NIK3i4", libSceCustomMusicSysCallWrapperUnknown_CQv51NIK3i4_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_DABJz_plus_GrAWg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DABJz+GrAWg", libSceCustomMusicSysCallWrapperUnknown_DABJz_plus_GrAWg_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_EaC0JWOBvy4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EaC0JWOBvy4", libSceCustomMusicSysCallWrapperUnknown_EaC0JWOBvy4_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_HNx_minus_bHhat18_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HNx-bHhat18", libSceCustomMusicSysCallWrapperUnknown_HNx_minus_bHhat18_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_J4_plus_14VzlTHM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("J4+14VzlTHM", libSceCustomMusicSysCallWrapperUnknown_J4_plus_14VzlTHM_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_JSZXadyt_minus_nk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JSZXadyt-nk", libSceCustomMusicSysCallWrapperUnknown_JSZXadyt_minus_nk_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_Kxq7i4lQjGg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Kxq7i4lQjGg", libSceCustomMusicSysCallWrapperUnknown_Kxq7i4lQjGg_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_L0MmfM0N_minus_y8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("L0MmfM0N-y8", libSceCustomMusicSysCallWrapperUnknown_L0MmfM0N_minus_y8_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_Mk6mbbOvaBM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Mk6mbbOvaBM", libSceCustomMusicSysCallWrapperUnknown_Mk6mbbOvaBM_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_NbAJfB19zUk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NbAJfB19zUk", libSceCustomMusicSysCallWrapperUnknown_NbAJfB19zUk_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_QEVVJH35Fis_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QEVVJH35Fis", libSceCustomMusicSysCallWrapperUnknown_QEVVJH35Fis_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_ROnMWfJ56t8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ROnMWfJ56t8", libSceCustomMusicSysCallWrapperUnknown_ROnMWfJ56t8_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_SneVIJUdrLw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SneVIJUdrLw", libSceCustomMusicSysCallWrapperUnknown_SneVIJUdrLw_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_Tr_minus_kghWgcoI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Tr-kghWgcoI", libSceCustomMusicSysCallWrapperUnknown_Tr_minus_kghWgcoI_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_VDAd516qud8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VDAd516qud8", libSceCustomMusicSysCallWrapperUnknown_VDAd516qud8_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_VTS_plus_zlRk_minus_wo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VTS+zlRk-wo", libSceCustomMusicSysCallWrapperUnknown_VTS_plus_zlRk_minus_wo_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_Xkm5tRxA0UQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Xkm5tRxA0UQ", libSceCustomMusicSysCallWrapperUnknown_Xkm5tRxA0UQ_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_Y3l3L6A4OYE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Y3l3L6A4OYE", libSceCustomMusicSysCallWrapperUnknown_Y3l3L6A4OYE_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_ZiO9aSV1ZKc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZiO9aSV1ZKc", libSceCustomMusicSysCallWrapperUnknown_ZiO9aSV1ZKc_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_bzddXxOnr30_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bzddXxOnr30", libSceCustomMusicSysCallWrapperUnknown_bzddXxOnr30_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_eTHTXNt0B2k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eTHTXNt0B2k", libSceCustomMusicSysCallWrapperUnknown_eTHTXNt0B2k_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_fsQhNY_plus_RJf8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fsQhNY+RJf8", libSceCustomMusicSysCallWrapperUnknown_fsQhNY_plus_RJf8_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_gxew9cxe_plus_xk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gxew9cxe+xk", libSceCustomMusicSysCallWrapperUnknown_gxew9cxe_plus_xk_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_hd0MG4ZvfRA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hd0MG4ZvfRA", libSceCustomMusicSysCallWrapperUnknown_hd0MG4ZvfRA_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_jETJjJLQtas_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jETJjJLQtas", libSceCustomMusicSysCallWrapperUnknown_jETJjJLQtas_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_jVD2jFJAQ28_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jVD2jFJAQ28", libSceCustomMusicSysCallWrapperUnknown_jVD2jFJAQ28_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_kso0cI0kq1U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kso0cI0kq1U", libSceCustomMusicSysCallWrapperUnknown_kso0cI0kq1U_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_lgRL6Oblmyg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lgRL6Oblmyg", libSceCustomMusicSysCallWrapperUnknown_lgRL6Oblmyg_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_mgSEL99WSwA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mgSEL99WSwA", libSceCustomMusicSysCallWrapperUnknown_mgSEL99WSwA_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_n0aVoomYuOM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("n0aVoomYuOM", libSceCustomMusicSysCallWrapperUnknown_n0aVoomYuOM_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_n4jZ2HjzXUo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("n4jZ2HjzXUo", libSceCustomMusicSysCallWrapperUnknown_n4jZ2HjzXUo_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_oaaW7CuQLV0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oaaW7CuQLV0", libSceCustomMusicSysCallWrapperUnknown_oaaW7CuQLV0_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_qqNVevi1LlY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qqNVevi1LlY", libSceCustomMusicSysCallWrapperUnknown_qqNVevi1LlY_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_sWVPeXELNss_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sWVPeXELNss", libSceCustomMusicSysCallWrapperUnknown_sWVPeXELNss_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_sak_plus_IpFmBkk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sak+IpFmBkk", libSceCustomMusicSysCallWrapperUnknown_sak_plus_IpFmBkk_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_y2Fu_plus_vFwEXo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("y2Fu+vFwEXo", libSceCustomMusicSysCallWrapperUnknown_y2Fu_plus_vFwEXo_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_yM_minus_etuyf_minus_2M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yM-etuyf-2M", libSceCustomMusicSysCallWrapperUnknown_yM_minus_etuyf_minus_2M_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_zRDEHOFDvzQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zRDEHOFDvzQ", libSceCustomMusicSysCallWrapperUnknown_zRDEHOFDvzQ_nid_no_patch);

int APS5_VABI libSceCustomMusicSysCallWrapperUnknown_zUgaEYLIysQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zUgaEYLIysQ", libSceCustomMusicSysCallWrapperUnknown_zUgaEYLIysQ_nid_no_patch);

}

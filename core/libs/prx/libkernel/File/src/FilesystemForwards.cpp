#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/FilesystemForwards.hpp"

namespace {

struct FilesystemForwardsRegistration {
    FilesystemForwardsRegistration() {
        static const FilesystemForwards_nid_no_patch forwards{
            sceKernelStat,
            sceKernelFstat,
            sceKernelOpen,
            sceKernelRead,
            sceKernelWrite,
            sceKernelClose,
            sceKernelMkdir,
            sceKernelUnlink,
            sceKernelRename,
            sceKernelTruncate_nid_postfix,
            sceKernelChmod_nid_postfix,
            sceKernelFchmod,
            sceKernelUtimes_nid_postfix,
            stat_nid_postfix,
            rmdir_nid_postfix,
        };
        RegisterFilesystemForwards_nid_no_patch(&forwards);
    }
    ~FilesystemForwardsRegistration() { RegisterFilesystemForwards_nid_no_patch(nullptr); }
    FilesystemForwardsRegistration(const FilesystemForwardsRegistration&) = delete;
    FilesystemForwardsRegistration& operator=(const FilesystemForwardsRegistration&) = delete;
};

const FilesystemForwardsRegistration registration;

}

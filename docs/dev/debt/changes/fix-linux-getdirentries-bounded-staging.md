# libkernel

## add Unknown function info

- [Linux getdirentries host staging](../../../core/libs/prx/libkernel/File/src/Stdio.cpp) (libkernel) - the Linux host `getdents64` call uses at most a 64 KiB temporary allocation even if the guest passes a larger positive `nbytes`. The returned guest record count can therefore be shorter than the supplied buffer, with the unread directory entries retained through host directory cookies for subsequent calls. The cap protects host memory against guest-controlled counts; whether a PS5 returns an equivalently short batch for large guest buffers is not measured.

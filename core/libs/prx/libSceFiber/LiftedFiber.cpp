#include "prx/libc/include/General.hpp"
#include <thread>

void LiftedFiberRequireHostThread(const std::thread::id hostThread) {
    if (hostThread != std::this_thread::get_id()) NotImplemented_nid_no_patch("resuming a lifted fiber on another thread");
}

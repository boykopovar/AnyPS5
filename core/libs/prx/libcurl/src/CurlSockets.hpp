#pragma once
#include "prx/libcurl/src/CurlHost.hpp"
#include <memory>
#include <unordered_map>

namespace CurlSockets {
struct Slot {
    void* function = nullptr;
    void* data = nullptr;
};
struct Easy {
    Slot open;
    Slot close;
    Slot option;
};
struct Socket;
struct Multi {
    Slot callback;
    std::unordered_map<curl_socket_t, std::shared_ptr<Socket>> views;
};
void BindEasy_nid_no_patch(CURL* easy, Easy* context);
CURLcode SetEasy_nid_no_patch(CURL* easy, Easy* context, CURLoption option, void* value);
void BindMulti_nid_no_patch(CURLM* multi, Multi* context);
CURLMcode SetMulti_nid_no_patch(CURLM* multi, Multi* context, CURLMoption option, void* value);
CURLMcode Action_nid_no_patch(CURLM* multi, Multi* context, int descriptor, int events, int* running, bool legacy);
CURLMcode Assign_nid_no_patch(CURLM* multi, Multi* context, int descriptor, void* value);
CURLMcode Fdset_nid_no_patch(CURLM* multi, Multi* context, guest_fd_set* read, guest_fd_set* write, guest_fd_set* except, int* maximum);
bool Add_nid_no_patch(guest_fd_set* set, int descriptor);
CURLMcode Wait_nid_no_patch(CURLM* multi, guest_curl_waitfd* extra, unsigned count, int timeout, int* ready, bool poll);
CURLcode Active_nid_no_patch(CURL* easy, CURLINFO info, void* output);
void Refresh_nid_no_patch(CURLM* multi, Multi* context);
}

extern "C" int CurlSocketNonblock_nid_no_patch(curl_socket_t socket, int nonblocking);
extern "C" int CurlHostNonblock_nid_no_patch(curl_socket_t socket, int nonblocking);

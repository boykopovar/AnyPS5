#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace NKWebKit {

namespace {

ProcessPool::InvalidMessageFunction g_invalidMessageFunction = nullptr;

}  // namespace

ProcessPool::InvalidMessageFunction ProcessPool::GetInvalidMessageFunction() {
    return g_invalidMessageFunction;
}

void ProcessPool::SetInvalidMessageFunction(InvalidMessageFunction function) {
    g_invalidMessageFunction = function;
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI WKContextSetClient(const void* contextRef, const void* client) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->clientConfigured = client != nullptr;
}

void APS5_VABI WKContextSetHistoryClient(const void* contextRef, const void* client) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->historyClientConfigured = client != nullptr;
}

void APS5_VABI WKContextSetInvalidMessageFunction(void (*function)(const void* messageName)) {
    NKWebKit::ProcessPool::SetInvalidMessageFunction(function);
}

}

#include "prx/libc/include/general/ThreadName.hpp"
#include <string>

int main() {
    constexpr const char* name = "AnyPS5 queue worker 0x2a";
    const HRESULT set = ThreadName::SetCurrent(name);
    if (set == S_FALSE) return 0;
    if (FAILED(set)) return 1;
    using GetThreadDescriptionFunction = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    const auto getThreadDescription = ThreadName::Resolve<GetThreadDescriptionFunction>("GetThreadDescription");
    if (getThreadDescription == nullptr) return 0;
    PWSTR description = nullptr;
    if (FAILED(getThreadDescription(GetCurrentThread(), &description))) return 2;
    const bool matches = description != nullptr && std::wstring(description) == L"AnyPS5 queue worker 0x2a";
    if (description != nullptr) LocalFree(description);
    return matches ? 0 : 3;
}

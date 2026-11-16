#include <cstdlib>

extern "C" {

int _ZNK15AbstractStorage12LocalContent13GetCapabilityEv();
int _ZNK15AbstractStorage12LocalContent10GetServiceEv();
int _ZN15AbstractStorage12LocalContent8SeekTimeElPl(long long, long*);
int _ZNK15AbstractStorage11LocalFolder10GetServiceEv();
int _ZNK15AbstractStorage12LocalService14GetServiceTypeEv();
int _ZNK15AbstractStorage12LocalStorage7GetStatEPNS_4StatE(long*);
int _ZNK15AbstractStorage12LocalStorage10GetServiceEv();
int _ZN15AbstractStorage12LocalStorage14GetStorageSizeEPlS1_S1_(long*, long*, long*);

}

namespace {

constexpr int kNotSupported = static_cast<int>(0x809E0005u);
constexpr int kError809E0002 = static_cast<int>(0x809E0002u);
constexpr unsigned long kProbeAddress = 0x1234;

void Require(bool value) {
    if (!value) std::abort();
}

void CheckConstants(long* a, long* b, long* c, long long offset) {
    Require(_ZNK15AbstractStorage12LocalContent13GetCapabilityEv() == 0x37C5A);
    Require(_ZNK15AbstractStorage12LocalContent10GetServiceEv() == 0);
    Require(_ZN15AbstractStorage12LocalContent8SeekTimeElPl(offset, a) == kNotSupported);
    Require(_ZNK15AbstractStorage11LocalFolder10GetServiceEv() == 0);
    Require(_ZNK15AbstractStorage12LocalService14GetServiceTypeEv() == 0);
    Require(_ZNK15AbstractStorage12LocalStorage7GetStatEPNS_4StatE(a) == kError809E0002);
    Require(_ZNK15AbstractStorage12LocalStorage10GetServiceEv() == 0);
    Require(_ZN15AbstractStorage12LocalStorage14GetStorageSizeEPlS1_S1_(a, b, c) == kError809E0002);
}

} // namespace

int main() {
    for (int call = 0; call < 3; ++call) {
        CheckConstants(nullptr, nullptr, nullptr, 0);
        long a = 7;
        long b = -3;
        long c = static_cast<long>(1ull << 40);
        CheckConstants(&a, &b, &c, 5);
        CheckConstants(&a, &b, &c, -1);
        CheckConstants(reinterpret_cast<long*>(kProbeAddress), reinterpret_cast<long*>(kProbeAddress + 8),
                       reinterpret_cast<long*>(kProbeAddress + 16), 5);
    }
    return 0;
}

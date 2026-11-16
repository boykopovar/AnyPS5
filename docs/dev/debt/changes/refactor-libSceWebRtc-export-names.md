# libSceWebRtc

## change Callable exports

- [Callable exports](../../../core/libs/prx/libSceWebRtc/Export.cpp) - 5 entries are import-resolution declarations only. Three are the `sce::np::rtc` functions whose mangled names hash to them: `-sx0eFixkek` is `sce::np::rtc::Terminate()`, `SRJUfO0JINA` is `sce::np::rtc::Initialize(sce::np::rtc::InitParameters const*)` and `ZZV+i+-wetM` is `sce::np::rtc::Context::Create()`; `Z7F8YLISPDI` and `z1bMECaLQhM` stay literal NID aliases. Return types, the `InitParameters` and `Context` layouts, and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented. Data exports require separate layout and ownership analysis.

## change SRJUfO0JINA

| `_ZN3sce2np3rtc10InitializeEPKNS1_14InitParametersE` | `SRJUfO0JINA` |

## change ZZV+i+-wetM

| `_ZN3sce2np3rtc7Context6CreateEv` | `ZZV+i+-wetM` |

# libSceNpCppWebApi

## add Functional

- [Activities::V1::UserActivities IntrusivePtr::operator->](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - probing `self` with `GuestCopyIn_nid_no_patch` and returning `nullptr` while preserving `errno` on null or unmapped guest pointers (`UserActivities`, `Activity`, `GetUsersActivitiesResponse`) is a host-safety fallback; the console module dereferences the `IntrusivePtr` directly and its fault behavior is not measured on a console.

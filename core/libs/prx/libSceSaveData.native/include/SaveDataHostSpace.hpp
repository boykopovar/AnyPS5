#ifndef CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_INCLUDE_SAVEDATAHOSTSPACE_HPP
#define CORE_LIBS_PRX_LIBSCESAVEDATANATIVE_INCLUDE_SAVEDATAHOSTSPACE_HPP

#include <cstdint>
#include <string>

using SaveDataFreeSpaceSource = std::uint64_t (*)(const std::string& path);

extern "C" void SetSaveDataFreeSpaceSourceForTest_nid_no_patch(SaveDataFreeSpaceSource source);

#endif

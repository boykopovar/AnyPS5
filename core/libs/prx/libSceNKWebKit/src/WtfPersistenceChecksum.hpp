#pragma once

#include <cstddef>

namespace NKWebKit::WtfAbi {

struct SHA1;
void PersistenceChecksumAdd_nid_no_patch(SHA1& checksum, const void* data, std::size_t size);

}  // namespace NKWebKit::WtfAbi

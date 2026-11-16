#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

enum CredentialPersistence : std::uint32_t {
    CredentialPersistenceNone,
    CredentialPersistenceForSession,
    CredentialPersistencePermanent
};

struct CredentialBase {
    WtfAbi::String user;
    WtfAbi::String password;
    CredentialPersistence persistence;
};

static_assert(sizeof(CredentialBase) == 24);
static_assert(offsetof(CredentialBase, user) == 0);
static_assert(offsetof(CredentialBase, password) == 8);
static_assert(offsetof(CredentialBase, persistence) == 16);

}

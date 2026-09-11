#ifndef RELINKER_DOMAIN_TYPES_HPP
#define RELINKER_DOMAIN_TYPES_HPP

#include <domain/Types.hpp>

namespace Relinker {

using FileByteOffset = Domain::FileByteOffset;
using VirtualAddress = Domain::VirtualAddress;
using ByteCount = Domain::ByteCount;
using ElfHeader = Domain::ElfHeader;
using ProgramHeader = Domain::ProgramHeader;
using SectionHeader = Domain::SectionHeader;
using NidReference = Domain::NidReference;
using CallSiteInfo = Domain::CallSiteInfo;
using SymbolExport = Domain::SymbolExport;
using DynamicTag = Domain::DynamicTag;
using RelinkerException = Domain::RelinkerException;
using SysVDynamicSection = Domain::SysVDynamicSection;
using CallRegistryEntry = Domain::CallRegistryEntry;

}

#endif

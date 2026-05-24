#pragma once

#include <set>
#include <unordered_set>

typedef int16_t ThreadIdT;
typedef int64_t EventIdT;
typedef int32_t ResourceIdT;
typedef int32_t SrcLocT;
typedef int64_t BinEvT;

typedef std::set<ResourceIdT> LocksetT;
typedef std::unordered_set<ResourceIdT> ULocksetT;

typedef std::unordered_set<ResourceIdT> UThreadSetT;
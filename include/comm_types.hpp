#pragma once

#include <set>
#include <unordered_set>

typedef int ThreadIdT;
typedef int TracePosT;
typedef int ResourceIdT;
typedef int SrcLocT;

typedef std::set<ResourceIdT> LocksetT;
typedef std::unordered_set<ResourceIdT> ULocksetT;

typedef std::unordered_set<ResourceIdT> UThreadSetT;
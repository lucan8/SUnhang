#pragma once

#include <unordered_set>
#include <stdint.h>

typedef int16_t ThreadIdT;
typedef int64_t EventIdT;
typedef int32_t ResourceIdT;
typedef int32_t SrcLocT;
typedef int64_t BinEvT;

typedef std::unordered_set<ResourceIdT> ULocksetT;
typedef std::unordered_set<ThreadIdT> UThreadSetT;
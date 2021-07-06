#pragma once

#ifndef ZAF_ACTOR_ID_TYPE
#define ZAF_ACTOR_ID_TYPE uint32_t
#endif

using ActorIdType = ZAF_ACTOR_ID_TYPE;

#ifndef ENABLE_PHMAP
#define ENABLE_PHMAP 0
#elif ENABLE_PHMAP
#include "parallel_hashmap/phmap.h"
#pragma message("Use phmap hash map and hash set")
#endif

#if ENABLE_PHMAP
template<typename ... ArgT>
using DefaultHashMap = phmap::flat_hash_map<ArgT ...>;
#else
#include <unordered_map>
template<typename ... ArgT>
using DefaultHashMap = std::unordered_map<ArgT ...>;
#endif

#if ENABLE_PHMAP
template<typename ... ArgT>
using DefaultHashSet = phmap::flat_hash_set<ArgT ...>;
#else
#include <unordered_set>
template<typename ... ArgT>
using DefaultHashSet = std::unordered_set<ArgT ...>;
#endif

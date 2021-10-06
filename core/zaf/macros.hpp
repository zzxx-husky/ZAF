#pragma once

#ifndef ZAF_PRINT_MACROS
  #define ZAF_PRINT_MACROS 0
#endif

#ifndef ZAF_ACTOR_ID_TYPE
  #define ZAF_ACTOR_ID_TYPE uint32_t
#endif

using ActorIdType = ZAF_ACTOR_ID_TYPE;

#ifndef ZAF_ACTOR_ID_MAX_LEN
  #define ZAF_ACTOR_ID_MAX_LEN 5
#endif

inline constexpr size_t ActorIdMaxLen = ZAF_ACTOR_ID_MAX_LEN;
inline constexpr size_t MaxActorId = []() {
  size_t max_id = 1;
  for (int i = 0; i < ActorIdMaxLen; i++) {
    max_id *= 10;
  }
  return max_id;
}();

#ifndef ENABLE_PHMAP
  #define ENABLE_PHMAP 0
#elif ENABLE_PHMAP
  #include "parallel_hashmap/phmap.h"
  #include "parallel_hashmap/btree.h"
  #if ZAF_PRINT_MACROS
    #pragma message("Use phmap hash map and hash set")
  #endif
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

#if ENABLE_PHMAP
  template<typename ... ArgT>
  using DefaultSortedMap = phmap::btree_map<ArgT ...>;
#else
  #include <map>
  template<typename ... ArgT>
  using DefaultSortedMap = std::map<ArgT ...>;
#endif

#if ENABLE_PHMAP
  template<typename ... ArgT>
  using DefaultSortedMultiMap = phmap::btree_multimap<ArgT ...>;
#else
  #include <map>
  template<typename ... ArgT>
  using DefaultSortedMultiMap = std::multimap<ArgT ...>;
#endif

#if ENABLE_PHMAP
  template<typename ... ArgT>
  using DefaultSortedSet = phmap::btree_set<ArgT ...>;
#else
  #include <set>
  template<typename ... ArgT>
  using DefaultSortedSet = std::set<ArgT ...>;
#endif

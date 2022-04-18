#pragma once

#include <sched.h>
#include <string>

#include "zaf_exception.hpp"

namespace zaf {
namespace thread {
inline static std::string get_name() {
  auto native_handle = pthread_self();
  std::string name;
  name.resize(16);
  auto rc = pthread_getname_np(native_handle, (char*)(name.data()), name.size());
  if (rc != 0) {
    throw ZAFException("Failed to get name of current thread. Error: ", rc);
  }
  for (int i = 0, n = name.size(); i < n; i++) {
    if (name[i] == 0) {
      name.resize(i);
      break;
    }
  }
  return name;
}

/**
 * Set the affinity of current thread to given `target_cpu`
 * After migrate the thread to the given cpu, reset the affinity to the original one
 * At the end, we migrate the thread but do not change the affinity
 */
[[maybe_unused]]
inline static void migrate_to_cpu(int target_cpu) {
  auto native_handle = pthread_self();
  std::string t_name = get_name();
  // auto old_cpu = sched_getcpu();
  cpu_set_t old_cpuset, cpuset;
  auto rc = pthread_getaffinity_np(native_handle, sizeof(cpu_set_t), &old_cpuset);
  if (rc != 0) {
    throw ZAFException("Failed to get cpu set for ", t_name, ". Error: ", rc);
  }
  CPU_ZERO(&cpuset);
  CPU_SET(target_cpu, &cpuset);
  // this is to move the thread to the dedicated core
  rc = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    throw ZAFException("Failed to set cpu affinity for ", t_name, ". Error: ", rc);
  }
  // reset cpuset of current thread, such that child thread will not be restricted to the same core
  rc = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &old_cpuset);
  if (rc != 0) {
    throw ZAFException("Failed to reset cpu affinity for ", t_name, ". Error: ", rc);
  }
}

inline static void set_name(const std::string& t_name) {
  auto native_handle = pthread_self();
  if (t_name.size() + 1 > 16) {
    throw ZAFException(t_name, " is too long. Thread name in linux is not "
      "allowed to be larger than 16, including the terminaing null byte.");
  }
  auto old_t_name = get_name(); 
  auto rc = pthread_setname_np(native_handle, t_name.c_str());
  if (rc != 0) {
    throw ZAFException("Failed to set the name of thread ", old_t_name,
      " to ", t_name, ". Error: ", rc);
  }
  if (get_name() != t_name) {
    throw ZAFException("Failed to set the name of thread ", old_t_name,
      " to ", t_name, ". Error: Thread name is not updated.");
  }
}
} // namespace thread
} // namespace zaf

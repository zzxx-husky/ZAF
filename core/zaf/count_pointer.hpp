#pragma once

#include <functional>
#include <utility>

namespace zaf {
// CountPointer is designed to be used within a single thread only.
// It uses unsigned as the counter type which is not thread-safe.
template<typename T>
class CountPointer {
public:
  // Initizalize with an existing pointer
  CountPointer(T* ptr = nullptr) {
    if (ptr) {
      value = ptr;
      count = new unsigned(1);
    }
  }

  // Construct a new pointer
  template<typename ... ArgT>
  explicit CountPointer(ArgT&& ... args):
    value(new T(std::forward<ArgT>(args)...)),
    count(new unsigned(1)) {
  }

  CountPointer(const CountPointer<T>& ptr) :
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    inc_count();
  }

  explicit CountPointer(CountPointer<T>&& ptr) :
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    inc_count();
    ptr = nullptr;
  }

  // give up the old one and create a new pointer
  CountPointer<T>& operator=(T* ptr) {
    dec_count();
    if (ptr) {
      count = new unsigned(1);
      value = ptr;
    } else {
      count = nullptr;
      value = nullptr;
    }
    return *this;
  }

  CountPointer<T>& operator=(const CountPointer<T>& ptr) {
    if (count != ptr.count) {
      dec_count();
      count = ptr.count;
      value = ptr.value;
      destructor = ptr.destructor;
      inc_count();
    }
    return *this;
  }

  CountPointer<T>& operator=(CountPointer<T>&& ptr) {
    if (count != ptr.count) {
      dec_count();
      count = ptr.count;
      value = ptr.value;
      destructor = ptr.destructor;
      ptr.count = nullptr;
      ptr.value = nullptr;
      ptr.destructor = nullptr;
    }
    return *this;
  }

  T* get() {
    return value;
  }

  const T* get() const {
    return value;
  }

  T& operator*() {
    return *value;
  }

  const T& operator*() const {
    return *value;
  }

  T* operator->() {
    return value;
  }

  const T* operator->() const {
    return value;
  }

  explicit operator bool() const {
    return value != nullptr;
  }

  operator T&() {
    return *value;
  }

  operator const T&() const {
    return *value;
  }

  ~CountPointer() {
    dec_count();
  }

  template<typename D>
  void set_destructor(D&& destructor) {
    this->destructor = std::forward<D>(destructor);
  }

  inline friend bool operator==(const CountPointer<T>& a, const CountPointer<T>& b) {
    // if value pointers are the same, count pointers should also be the same
    return a.value == b.value;
  }

protected:
  T* value = nullptr;
  unsigned* count = nullptr;
  std::function<void()> destructor = nullptr;

  void inc_count() {
    if (count) {
      ++*count;
    }
  }

  void dec_count() {
    if (count && --*count == 0) {
      if (destructor) {
        destructor();
        destructor = nullptr;
      }
      delete count;
      count = nullptr;
      delete value;
      value = nullptr;
    }
  }
};

template<typename T, typename ... ArgT>
CountPointer<T> make_count(ArgT&& ... args) {
  return {std::forward<ArgT>(args) ...};
}
} // namespace zaf 

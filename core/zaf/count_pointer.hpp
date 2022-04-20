#pragma once

#include <functional>
#include <utility>

namespace zaf {
// CountPointer is designed to be used within a single thread only.
// It uses unsigned as the counter type which is not thread-safe.
template<typename T>
class CountPointer {
public:
  // same as null pointer
  CountPointer() = default;

  // Initizalize with an existing pointer
  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer(U* ptr) {
    if (ptr) {
      value = ptr;
      count = new unsigned(1);
    }
  }

  CountPointer(std::nullptr_t) {
    /* do nothing */
  }

  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer(const CountPointer<U>& ptr):
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    inc_count();
  }

  CountPointer(const CountPointer<T>& ptr):
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    inc_count();
  }

  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer(CountPointer<U>&& ptr) :
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    ptr.value = nullptr;
    ptr.count = nullptr;
    ptr.destructor = nullptr;
  }

  CountPointer(CountPointer<T>&& ptr) :
    value(ptr.value),
    count(ptr.count),
    destructor(ptr.destructor) {
    ptr.value = nullptr;
    ptr.count = nullptr;
    ptr.destructor = nullptr;
  }

  // give up the old one and create a new pointer
  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer<T>& operator=(U* ptr) {
    dec_count();
    if (ptr) {
      count = new unsigned(1);
      value = ptr;
    } else {
      count = nullptr;
      value = nullptr;
    }
    destructor = nullptr;
    return *this;
  }

  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer<T>& operator=(const CountPointer<U>& ptr) {
    if (count != ptr.count) {
      dec_count();
      count = ptr.count;
      value = ptr.value;
      destructor = ptr.destructor;
      inc_count();
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

  template<typename U,
    std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
  CountPointer<T>& operator=(CountPointer<U>&& ptr) {
    if (this == &ptr) {
      return *this;
    }
    dec_count();
    if (count != ptr.count) {
      count = ptr.count;
      value = ptr.value;
      destructor = ptr.destructor;
    }
    ptr.count = nullptr;
    ptr.value = nullptr;
    ptr.destructor = nullptr;
    return *this;
  }

  CountPointer<T>& operator=(CountPointer<T>&& ptr) {
    if (this == &ptr) {
      return *this;
    }
    dec_count();
    if (count != ptr.count) {
      count = ptr.count;
      value = ptr.value;
      destructor = ptr.destructor;
    }
    ptr.count = nullptr;
    ptr.value = nullptr;
    ptr.destructor = nullptr;
    return *this;
  }

  CountPointer<T>& operator=(std::nullptr_t) {
    dec_count();
    count = nullptr;
    value = nullptr;
    destructor = nullptr;
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

  operator bool() const {
    return value != nullptr;
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
  return {new T(std::forward<ArgT>(args) ...)};
}
} // namespace zaf 

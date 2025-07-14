#pragma once

#include <cstdlib>
#include <new>

namespace ts {
  template<typename T>
  T *alloc(const std::size_t size) {
    auto p = std::aligned_alloc(sizeof(T) * size, alignof(T));
    if (!p) {
      throw std::bad_alloc();
    }
    return static_cast<T *>(p);
  }

  template<typename T>
  void free(T *p) {
    if (p) {
      std::free(p);
    }
  }
}

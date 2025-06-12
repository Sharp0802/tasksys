#pragma once

#include <cstdint>
#include <new>

namespace ts {
  template<typename T>
  T *alloc(std::size_t size) {
    return static_cast<T *>(operator new(sizeof(T) * size, std::align_val_t{alignof(T)}));
  }

  template<typename T>
  void free(T *p) {
    operator delete(p, std::align_val_t{alignof(T)});
  }
}

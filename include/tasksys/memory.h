#pragma once

#include <cstdint>
#include <new>

namespace ts {
  template<typename T>
  T *alloc(std::size_t size) {
    auto *p = static_cast<T *>(operator new(sizeof(T) * size, std::align_val_t{alignof(T)}));
#if _DEBUG
    if (p == nullptr) {
      throw std::runtime_error("allocation failed!");
    }
#endif
    return p;
  }

  template<typename T>
  void free(T *p) {
#if _DEBUG
    if (p == nullptr) {
      return;
    }
#endif
    operator delete(p, std::align_val_t{alignof(T)});
  }
}

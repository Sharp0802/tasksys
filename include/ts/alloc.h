#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

namespace ts {
  template<typename T>
  T *alloc(const size_t cnt) {
    auto p = std::malloc(sizeof(T) * cnt);
#if !NDEBUG
    if (!p) {
      throw std::bad_alloc();
    }
    std::memset(p, 0xCC, sizeof(T) * cnt);
#endif

    return p;
  }

  inline void free(void *p) {
    std::free(p);
  }
}

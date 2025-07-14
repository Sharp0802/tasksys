#pragma once

#include <atomic>
#include <optional>

#include "Job.h"

namespace ts {
  class ChaseLevDeque {
    static constexpr size_t N = 512;

    static_assert(N > 0 && (N & N - 1) == 0, "N must be a power of two.");

    static constexpr size_t ALIGN = std::hardware_destructive_interference_size;
    static constexpr size_t MASK = N - 1;

    JobSlot _buffer[N];

    alignas(ALIGN) std::atomic_size_t _top;
    alignas(ALIGN) std::atomic_size_t _bottom;

  public:
    ChaseLevDeque(const ChaseLevDeque &) = delete;
    ChaseLevDeque &operator=(const ChaseLevDeque &) = delete;

    ChaseLevDeque() : _buffer{}, _top(0), _bottom(0) {
    }

    std::optional<Job> pop();
    std::optional<Job> steal();
    bool push(const Job &job);
  };
}

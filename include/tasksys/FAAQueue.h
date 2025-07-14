#pragma once

#include <optional>

#include "Job.h"

namespace ts {
  class FAAQueue {
    static constexpr size_t N = 512;

    static_assert(N > 0 && (N & N - 1) == 0, "N must be a power of two.");

    static constexpr size_t ALIGN = std::hardware_destructive_interference_size;
    static constexpr size_t MASK = N - 1;

    JobSlot _buffer[N];

    alignas(ALIGN) std::atomic_size_t _head;
    alignas(ALIGN) std::atomic_size_t _commit;
    alignas(ALIGN) std::atomic_size_t _tail;

  public:
    bool push(const Job& job);
    std::optional<Job> pop();
  };
}

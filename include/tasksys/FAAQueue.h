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
    bool push(const Job& job) {
      auto head = _head.load(std::memory_order_relaxed);
      while (true) {
        if (const auto tail = _tail.load(std::memory_order_acquire); head - tail >= N) {
          // buffer is full
          return false;
        }

        const auto new_head = head + 1;

        if (!_head.compare_exchange_weak(
          head, new_head,
          std::memory_order_relaxed,
          std::memory_order_relaxed)) {
          // lose the race
          continue;
        }

        // won the race; store job
        _buffer[head & MASK].store(job, std::memory_order_relaxed);

        size_t copy_head;
        do {
          copy_head = head;
        } while (!_commit.compare_exchange_weak(
          copy_head, new_head,
          std::memory_order_acq_rel,
          std::memory_order_relaxed));

        return true;
      }
    }

    std::optional<Job> pop() {
      auto tail = _tail.load(std::memory_order_relaxed);
      while (true) {
        if (const auto commit = _commit.load(std::memory_order_acquire); tail == commit) {
          return std::nullopt;
        }

        // load job before race: can be overwritten by push
        const auto job = _buffer[tail & MASK].load(std::memory_order_relaxed);

        if (!_tail.compare_exchange_weak(
          tail, tail + 1,
          std::memory_order_relaxed,
          std::memory_order_relaxed)) {
          // lose the race
          continue;
        }

        return job;
      }
    }
  };
}

#pragma once

#include "LocalQueue.h"

namespace ts {
  /*
   * FFA-queue
   */
  class GlobalQueue {
    struct alignas(64) Slot {
      static_assert(sizeof(Job) < 64);

      Job data;
      char __pad[64 - sizeof(data)];
    };

    size_t _mask;

    alignas(64) std::unique_ptr<Slot[]> _array;
    alignas(64) std::atomic_size_t _head;
    alignas(64) std::atomic_size_t _tail;

  public:
    explicit GlobalQueue(size_t size);

    bool push(const Job &job);
    bool pop(Job *job);
  };
}

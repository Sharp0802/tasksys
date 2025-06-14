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

    alignas(64) Slot *_array;
    alignas(64) std::atomic_size_t _head;
    alignas(64) std::atomic_size_t _tail;

  public:
    GlobalQueue(const GlobalQueue &) = delete;
    GlobalQueue &operator=(const GlobalQueue &) = delete;

    explicit GlobalQueue(size_t size);
    ~GlobalQueue();

    bool push(const Job &job);
    bool pop(Job *job);
  };
}

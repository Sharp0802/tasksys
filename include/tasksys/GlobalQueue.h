#pragma once

#include "LocalQueue.h"

namespace ts {
  class GlobalQueue {
    size_t _mask;

    alignas(64) Job *_array;
    alignas(64) std::atomic_size_t _head;
    alignas(64) std::atomic_size_t _prepared;
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

#pragma once

#include "workqueue.h"
#include <random>

namespace ts {
  struct alignas(64) AlignedWorkQueue : WorkQueue {
    using WorkQueue::WorkQueue;
  };

  class ThreadPool {
    static constexpr std::align_val_t ALIGN = std::align_val_t{alignof(AlignedWorkQueue)};

    AlignedWorkQueue *_queues;
    size_t _pool_size;

    std::random_device _rng;
    std::mt19937 _mt;
    std::uniform_int_distribution<size_t> _dis;

  public:
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(size_t pool_size, size_t queue_size)
        : _pool_size(pool_size),
          _mt(_rng()),
          _dis(0, pool_size - 1) {
      _queues = static_cast<AlignedWorkQueue *>(operator new[](sizeof *_queues * pool_size, ALIGN));

      size_t i = 0;
      try {
        for (; i < pool_size; ++i) {
          new(_queues + i) AlignedWorkQueue(queue_size);
        }
      } catch (...) {
        for (size_t j = 0; j < i; ++j) {
          _queues[j].~AlignedWorkQueue();
        }

        operator delete[]((void *) _queues, ALIGN);
        throw;
      }
    }

    ~ThreadPool() {
      stop();

      for (size_t i = 0; i < _pool_size; ++i) {
        _queues[i].~AlignedWorkQueue();
      }

      operator delete[]((void *) _queues, ALIGN);
    }

    void stop() {
      for (size_t i = 0; i < _pool_size; ++i) {
        _queues[i].stop();
      }
    }

    void push(void(*fn)(void *), void *data) {
      size_t sample_number = std::min<size_t>(3, _pool_size);

      AlignedWorkQueue* queues[sample_number];
      for (auto i = 0; i < sample_number; ++i) {
        queues[i] = &_queues[_dis(_mt)];
      }

      AlignedWorkQueue *queue = *queues;
      size_t pressure = (*queues)->pressure();

      for (auto i = 0; i < sample_number; ++i) {
        auto current = queues[i]->pressure();
        if (current > pressure) continue;

        queue = queues[i];
        pressure = current;
      }

      queue->push(fn, data);
    }
  };
}

#include "tasksys/GlobalQueue.h"
#include <vector>
#include <execution>

ts::GlobalQueue::GlobalQueue(size_t size) : _mask(size - 1) {
  assert(std::popcount(size) == 1);

  _array = std::make_unique<Slot[]>(size);
}

bool ts::GlobalQueue::push(const ts::Job &job) {
  const size_t capacity = _mask + 1;
  size_t head, tail = _tail.load(std::memory_order_relaxed);

  while (true) {
    head = _head.load(std::memory_order_acquire);
    if (tail - head >= capacity)
      return false;

    if (_tail.compare_exchange_weak(
        tail, tail + 1,
        std::memory_order_acq_rel,
        std::memory_order_relaxed))
    {
      _mm_prefetch(
          &_array[(tail + 1) & _mask],
          _MM_HINT_T0
      );

      _array[tail & _mask].data = job;
      return true;
    }
  }
}

bool ts::GlobalQueue::pop(ts::Job *job) {
  size_t tail, head = _head.load(std::memory_order_relaxed);

  while (true) {
    tail = _tail.load(std::memory_order_acquire);
    if (tail == head) {
      return false;
    }

    if (_head.compare_exchange_weak(
        head, head + 1,
        std::memory_order_acq_rel,
        std::memory_order_relaxed))
    {
      _mm_prefetch(
          &_array[(head + 1) & _mask],
          _MM_HINT_T0
      );

      *job = _array[head & _mask].data;
      return true;
    }
  }
}
